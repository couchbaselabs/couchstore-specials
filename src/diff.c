/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "config.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <libcouchstore/couch_db.h>
#include <snappy-c.h>
#include "bitfield.h"
#include "internal.h"

// Huh?
#define DEBUG
#include "util.h"
// These functions stolen from util.cc (which is now C++ no non-trivial to
// #include it into this C source file).
int ebin_cmp(const sized_buf *e1, const sized_buf *e2)
{
    size_t size;
    if (e2->size < e1->size) {
        size = e2->size;
    } else {
        size = e1->size;
    }

    int cmp = memcmp(e1->buf, e2->buf, size);
    if (cmp == 0) {
        if (size < e2->size) {
            return -1;
        } else if (size < e1->size) {
            return 1;
        }
    }
    return cmp;
}

void report_error(couchstore_error_t errcode, const char* file, int line) {
    fprintf(stderr, "Couchstore error `%s' at %s:%d\r\n", \
            couchstore_strerror(errcode), file, line);
}
// End stolen functions.

typedef struct itercur {
    DocInfo** cursor;
    uint64_t itemcount;
} itercur;

int fcallback(Db* db, DocInfo* info, void* ctx) {
    itercur* cur = (itercur*) ctx;
    *cur->cursor = info;
    cur->cursor++;
    cur->itemcount++;
    // Don't free the docinfo!
    return 1;
}

static void printsb(const sized_buf *sb)
{
    if (sb->buf == NULL) {
        printf("null\n");
        return;
    }
    printf("%.*s\n", (int) sb->size, sb->buf);
}

typedef struct {
    raw_64 cas;
    raw_32 expiry;
    raw_32 flags;
} CouchbaseRevMeta;

//metadata only compare
int check_match_info(DocInfo* a, DocInfo *b) {
    uint64_t acas, bcas;
    const CouchbaseRevMeta* meta;
    meta = (const CouchbaseRevMeta*)a->rev_meta.buf;
    acas = decode_raw64(meta->cas);
    meta = (const CouchbaseRevMeta*)b->rev_meta.buf;
    bcas = decode_raw64(meta->cas);
    return (a->rev_seq == b->rev_seq &&
            acas == bcas);
}

void dump_item(Db* db, DocInfo* docinfo) {
    // largely lifted from dbdump
    uint64_t cas; uint32_t expiry, flags;
    Doc *doc = NULL;
    printf("  Doc ID: ");
    printsb(&docinfo->id);
    if (docinfo->db_seq > 0) {
        printf("     seq: %"PRIu64"\n", docinfo->db_seq);
    }
    printf("     rev: %"PRIu64"\n", docinfo->rev_seq);
    printf("     content_meta: %d\n", docinfo->content_meta);
    if (docinfo->rev_meta.size == sizeof(CouchbaseRevMeta)) {
        const CouchbaseRevMeta* meta = (const CouchbaseRevMeta*)docinfo->rev_meta.buf;
        cas = decode_raw64(meta->cas);
        expiry = decode_raw32(meta->expiry);
        flags = decode_raw32(meta->flags);
        printf("     cas: %"PRIu64", expiry: %"PRIu32", flags: %"PRIu32"\n", cas, expiry, flags);
    }
    if (docinfo->deleted) {
        printf("     doc deleted\n");
    }

    couchstore_error_t docerr = couchstore_open_doc_with_docinfo(db, docinfo, &doc, DECOMPRESS_DOC_BODIES);
    if(docerr != COUCHSTORE_SUCCESS) {
        printf("     could not read document body: %s\n", couchstore_strerror(docerr));
    } else if (doc && (docinfo->content_meta & COUCH_DOC_IS_COMPRESSED)) {
        printf("     data: (snappy) ");
        printsb(&doc->data);
    } else if(doc) {
        printf("     data: ");
        printsb(&doc->data);
    }
    printf("\n");

    couchstore_free_document(doc);
}

couchstore_error_t drive_diff(Db* f1h, Db* f2h) {
    couchstore_error_t errcode = COUCHSTORE_SUCCESS;

    DocInfo **f1infos = NULL;
    DocInfo **f2infos = NULL;

    DbInfo f1info;
    error_pass(couchstore_db_info(f1h, &f1info));
    uint64_t f1entries = f1info.doc_count + f1info.deleted_count;

    DbInfo f2info;
    error_pass(couchstore_db_info(f2h, &f2info));
    uint64_t f2entries = f2info.doc_count + f2info.deleted_count;

    f1infos = calloc(sizeof(DocInfo*), f1entries);
    itercur f1cur;
    f1cur.cursor = f1infos;
    f1cur.itemcount = 0;
    couchstore_all_docs(f1h, NULL, 0, fcallback, &f1cur);

    f2infos = calloc(sizeof(DocInfo*), f2entries);
    itercur f2cur;
    f2cur.cursor = f2infos;
    f2cur.itemcount = 0;
    couchstore_all_docs(f2h, NULL, 0, fcallback, &f2cur);

    printf("Found %"PRIu64" entries in %s\n", f1entries, f1h->file.path);
    printf("Found %"PRIu64" entries in %s\n\n", f2entries, f2h->file.path);

    assert(f1entries == f1cur.itemcount);
    assert(f2entries == f2cur.itemcount);

    uint64_t only2 = 0, only1 = 0, diff = 0, same = 0;

    // merge-diff the metadata
    // (it's sorted so we can scrub through a list and any time we scrub one cursor past the other,
    //  we just pull the one that's behind forward to match or exceed the first)
    f2cur.cursor = f2infos;
    for(f1cur.cursor = f1infos;
            f1cur.cursor < f1infos + f1entries &&
            f2cur.cursor < f2infos + f2entries;
            f1cur.cursor++) {
        int cmp = ebin_cmp(&(*f1cur.cursor)->id, &(*f2cur.cursor)->id);
        if(cmp > 0) {
            while(f2cur.cursor < f2infos + f2entries) {
                int icmp = ebin_cmp(&(*f1cur.cursor)->id, &(*f2cur.cursor)->id);
                if(icmp > 0) {
                    only2++;
                    printf("Entry only in %s\n", f2h->file.path);
                    dump_item(f2h, *f2cur.cursor);
                    f2cur.cursor++;
                }
                if(icmp <= 0) break;
            }
            if(f2cur.cursor >= f2infos + f2entries) break;
        }
        cmp = ebin_cmp(&(*f1cur.cursor)->id, &(*f2cur.cursor)->id);
        if(cmp < 0) {
            only1++;
            printf("Entry only in %s\n", f1h->file.path);
            dump_item(f1h, *f1cur.cursor);
        }
        if(cmp == 0) {
            if(!check_match_info(*f1cur.cursor, *f2cur.cursor)) {
                diff++;
                printf("(+-) Entry differs between files\n");
                printf("  in %s:\n", f1h->file.path);
                dump_item(f1h, *f1cur.cursor);
                printf("  in %s:\n", f2h->file.path);
                dump_item(f2h, *f2cur.cursor);
            } else {
                same++;
            }
            //Entry in both
            f2cur.cursor++;
        }
    }
    while(f1cur.cursor < f1infos + f1entries) {
        only1++;
        printf("Entry only in %s\n", f1h->file.path);
        dump_item(f1h, *f1cur.cursor);
        f1cur.cursor++;
    }
    while(f2cur.cursor < f2infos + f2entries) {
        only2++;
        printf("Entry only in %s\n", f2h->file.path);
        dump_item(f2h, *f2cur.cursor);
        f2cur.cursor++;
    }

    printf("%"PRIu64" entries only in %s\n", only1, f1h->file.path);
    printf("%"PRIu64" entries only in %s\n", only2, f2h->file.path);
    printf("%"PRIu64" entries in both files differed\n", diff);
    printf("%"PRIu64" entries were similar\n", same);
    printf("%"PRIu64" total differences\n", diff + only1 + only2);

cleanup:
    if(f1infos) {
        for(f1cur.cursor = f1infos; f1cur.cursor < f1infos + f1entries; f1cur.cursor++) {
            couchstore_free_docinfo(*f1cur.cursor);
        }
        free(f1infos);
    }
    if(f2infos) {
        for(f2cur.cursor = f2infos; f2cur.cursor < f2infos + f2entries; f2cur.cursor++) {
            couchstore_free_docinfo(*f2cur.cursor);
        }
        free(f2infos);
    }
    return errcode;
}

int main(int argc, char **argv)
{
    couchstore_error_t errcode = COUCHSTORE_SUCCESS;

    if (argc < 3) {
        printf("%s <file1.couch> <file2.couch>\n", argv[0]);
        return 1;
    }
    Db *f1h = NULL;
    error_pass(couchstore_open_db(argv[1], COUCHSTORE_OPEN_FLAG_RDONLY, &f1h));
    Db *f2h = NULL;
    error_pass(couchstore_open_db(argv[2], COUCHSTORE_OPEN_FLAG_RDONLY, &f2h));

    drive_diff(f1h, f2h);

cleanup:
    if (f1h) {
        couchstore_close_db(f1h);
    }
    if (f2h) {
        couchstore_close_db(f2h);
    }
    return 0;
}

