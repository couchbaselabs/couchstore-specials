from couchstore import CouchStore, DocumentInfo, SizedBuf
import os
import struct

REV_META_PACK = ">QII"

def insert(db, key, rev, cas):
    info = DocumentInfo(key)
    info.revSequence = rev
    # cas, exp, flags
    info.revMeta = str(struct.pack(REV_META_PACK, 1, 2, 3))
    info.deleted = False
    return db.save(info, "this would be a value")

def main():
    dba = CouchStore("file1.couch", 'c');
    dbb = CouchStore("file2.couch", 'c');

    insert(dba, "foo", 1, 1000)
    insert(dba, "bar", 1, 1000)
    insert(dba, "baz", 1, 1000)
    insert(dba, "neat", 1, 1000)
    insert(dba, "car", 1, 1000)
    insert(dbb, "shelf", 9, 4484)
    insert(dba, "zoop", 1, 1000)
    insert(dba, "zzhuh", 1, 1000)

    insert(dbb, "foo", 1, 1000)
    insert(dbb, "shelf", 8, 9393)
    insert(dbb, "baz", 1, 1000)
    insert(dbb, "breeze", 1, 1000)
    insert(dbb, "neat", 8, 9393)
    insert(dbb, "fear", 1, 1000)
    insert(dbb, "zoop", 1, 1000)
    insert(dba, "zzbag", 2, 1000)
    dba.commit()
    dbb.commit()
    dba.close()
    dbb.close()

if __name__ == "__main__":
    main()

