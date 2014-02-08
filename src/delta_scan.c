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
#include "util.h"

typedef struct {
    raw_64 cas;
    raw_32 expiry;
    raw_32 flags;
} CouchbaseRevMeta;

static void printjquote(const sized_buf *sb)
{
    const char* i = sb->buf;
    const char* end = sb->buf + sb->size;
    if (sb->buf == NULL) {
        return;
    }
    for(; i < end; i++) {
        if(*i > 31 && *i != '\"' && *i != '\\') {
            fputc(*i, stdout);
        } else {
            fputc('\\', stdout);
            switch(*i)
            {
                case '\\': fputc('\\', stdout);break;
                case '\"': fputc('\"', stdout);break;
                case '\b': fputc('b', stdout);break;
                case '\f': fputc('f', stdout);break;
                case '\n': fputc('n', stdout);break;
                case '\r': fputc('r', stdout);break;
                case '\t': fputc('t', stdout);break;
                default:
                           printf("uFFFD");
            }
        }
    }
}

typedef struct {
    Db* current;
    Db* previous;
} DeltaCtx;

static void doc_dump(Db *db, DocInfo *docinfo)
{
    Doc *doc = NULL;
    uint64_t cas;
    uint32_t expiry, flags;
    couchstore_error_t docerr;

    printf("{\"db_seq\":\"%"PRIu64"\"", docinfo->db_seq);
    if (docinfo->rev_meta.size == sizeof(CouchbaseRevMeta)) {
        const CouchbaseRevMeta* meta = (const CouchbaseRevMeta*)docinfo->rev_meta.buf;
        cas = decode_raw64(meta->cas);
        expiry = decode_raw32(meta->expiry);
        flags = decode_raw32(meta->flags);
        printf(",\"cas\":\"%"PRIu64"\",\"expiry\":%"PRIu32",\"flags\":%"PRIu32"", cas, expiry, flags);
    }
    if (docinfo->deleted) {
        printf(",\"deleted\":true");
    }

    printf(",\"rev\":%"PRIu64",\"cmeta\":%d", docinfo->rev_seq, docinfo->content_meta);

    if(getenv("INCLUDE_DOC_BODY") != NULL) {
        docerr = couchstore_open_doc_with_docinfo(db, docinfo, &doc, 0);
        if(docerr != COUCHSTORE_SUCCESS) {
            printf(",\"data\":null");
        } else if (doc && (docinfo->content_meta & COUCH_DOC_IS_COMPRESSED)) {
            size_t rlen;
            char *decbuf;
            size_t uncompr_len;

            snappy_uncompressed_length(doc->data.buf, doc->data.size, &rlen);
            decbuf = malloc(rlen);
            snappy_uncompress(doc->data.buf, doc->data.size, decbuf, &uncompr_len);
            sized_buf uncompr;
            uncompr.buf = decbuf;
            uncompr.size = uncompr_len;
            printf(",\"snappy\":true,\"data\":\"");
            printjquote(&uncompr);
            printf("\"");
            free(decbuf);
        } else if(doc) {
            printf(",\"data\":\"");
            printjquote(&doc->data);
            printf("\"");
        }
    }
    printf("}");

    couchstore_free_document(doc);
}

static int doc_db(Db *db, DocInfo *docinfo, void *ctx)
{
    DocInfo *old;
    DeltaCtx *dc = (DeltaCtx*) ctx;
    printf("{\"id\":\"");
    printjquote(&docinfo->id);
    printf("\",\"new\":");
    doc_dump(db, docinfo);
    printf(",\"previous\":");
    if(couchstore_docinfo_by_id(dc->previous, docinfo->id.buf, docinfo->id.size, &old) == COUCHSTORE_SUCCESS) {
        doc_dump(dc->previous, old);
    } else {
        printf("null");
    }
    printf("}");
    if(docinfo->db_seq < db->header.update_seq) { printf(","); }
    return 0;
}

int main(int argc, char **argv)
{
    couchstore_error_t errcode = COUCHSTORE_SUCCESS;
    DeltaCtx dc = {NULL, NULL};
    int count = 0;

    if (argc < 2) {
        fprintf(stderr, "need a file\n");
        return 1;
    }

    error_pass(couchstore_open_db(argv[1], COUCHSTORE_OPEN_FLAG_RDONLY, &dc.current));
    error_pass(couchstore_open_db(argv[1], COUCHSTORE_OPEN_FLAG_RDONLY, &dc.previous));
    while(couchstore_rewind_db_header(dc.previous) == COUCHSTORE_SUCCESS) {
        count++;
        // Iterate the newer handle since the update_seq of the previous handle

        printf("{\"pos\":\"%"PRIu64"\",\"old_seq\":\"%"PRIu64"\",\"new_seq\":\"%"PRIu64"\",\"changes\":[",
                dc.current->header.position, dc.previous->header.update_seq, dc.current->header.update_seq);
        couchstore_changes_since(dc.current, dc.previous->header.update_seq + 1, 0, doc_db, &dc);
        printf("]");
        LocalDoc* vbstate;
        if(couchstore_open_local_document(dc.current, "_local/vbstate", 14, &vbstate) == COUCHSTORE_SUCCESS) {
            printf(",\"vbstate\":\"");
            printjquote(&vbstate->json);
            printf("\"");
            couchstore_free_local_document(vbstate);
        }
        printf("}");
        printf("\n");
        error_pass(couchstore_rewind_db_header(dc.current));
    }

    fprintf(stderr, "Total headers found: %d\n", count);

cleanup:
    if (errcode != COUCHSTORE_SUCCESS) {
        fprintf(stderr, "Error: %s", couchstore_strerror(errcode));
    }
    couchstore_close_db(dc.current);
    if (errcode != COUCHSTORE_SUCCESS) {
        exit(EXIT_FAILURE);
    } else {
        exit(EXIT_SUCCESS);
    }
}
