# libcouchstore
# Copyright (C) 2010, 2011 Couchbase, Inc
# All rights reserved.
#
#   Licensed under the Apache License, Version 2.0 (the "License");
#   you may not use this file except in compliance with the License.
#   You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#   Unless required by applicable law or agreed to in writing, software
#   distributed under the License is distributed on an "AS IS" BASIS,
#   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#   See the License for the specific language governing permissions and
#   limitations under the License.
ACLOCAL_AMFLAGS = -I m4 --force

bin_PROGRAMS = couch_dbdump couch_dbinfo couch_compact couch_viewgen couch_diff
EXTRA_DIST = python LICENSE README.md

if WINDOWS
ICU_LOCAL_LIBS=-licuuc -licudt -licuin
else
ICU_LOCAL_LIBS=-licuuc -licudata -licui18n
endif

pkginclude_HEADERS = \
                     include/libcouchstore/couch_db.h \
                     include/libcouchstore/couch_index.h \
                     include/libcouchstore/couch_common.h \
                     include/libcouchstore/error.h \
                     include/libcouchstore/file_ops.h \
                     include/libcouchstore/visibility.h

lib_LTLIBRARIES = libcouchstore.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcouchstore.pc

noinst_LTLIBRARIES = librfc1321.la libbyteswap.la

libbyteswap_la_SOURCES = src/byteswap.c

librfc1321_la_SOURCES = \
                            src/rfc1321/md5c.c \
                            src/rfc1321/md5.h \
                            src/rfc1321/global.h
librfc1321_la_CFLAGS =  $(AM_CFLAGS) ${NO_WERROR}

libcouchstore_la_SOURCES = \
                            config_static.h \
                            src/arena.c \
                            src/arena.h \
                            src/bitfield.h \
                            src/btree_modify.c \
                            src/btree_read.c \
                            src/collate_json.c \
                            src/collate_json.h \
                            src/couch_btree.h \
                            src/couch_db.c \
                            src/couch_save.c \
                            src/crc32.c \
                            src/crc32.h \
                            src/couch_file_read.c \
                            src/couch_file_write.c \
                            src/db_compact.c \
                            src/fatbuf.h \
                            src/internal.h \
                            src/iobuffer.c \
                            src/iobuffer.h \
                            src/json_reduce.c \
                            src/json_reduce.h \
                            src/llmsort.c \
                            src/tree_writer.c \
                            src/tree_writer.h \
                            src/mergesort.c \
                            src/mergesort.h \
                            src/node_types.c \
                            src/node_types.h \
                            src/reduces.c \
                            src/reduces.h \
                            src/strerror.c \
                            src/util.c \
                            src/util.h \
                            src/couch_index.c


libcouchstore_la_LDFLAGS = $(AM_LDFLAGS) $(ICU_LOCAL_LDFLAGS) -version-info $(LIBCOUCHSTORE_API_CURRENT):$(LIBCOUCHSTORE_API_REVISION):$(LIBCOUCHSTORE_API_AGE) -no-undefined -lsnappy -lpthread

if WINDOWS
libcouchstore_la_SOURCES += src/os_win.c
libcouchstore_la_LDFLAGS += -lws2_32
else
libcouchstore_la_SOURCES += src/os.c
endif

libcouchstore_la_CFLAGS = $(AM_CFLAGS) $(ICU_LOCAL_CFLAGS) -DLIBCOUCHSTORE_INTERNAL=1 -Wstrict-aliasing=2 -Werror=strict-aliasing -pedantic
libcouchstore_la_LIBADD = librfc1321.la libbyteswap.la $(ICU_LOCAL_LIBS)

couch_dbdump_SOURCES = src/dbdump.c
couch_dbdump_DEPENDENCIES = libcouchstore.la
couch_dbdump_CFLAGS = $(AM_CFLAGS) -D__STDC_FORMAT_MACROS
couch_dbdump_LDADD = libcouchstore.la libbyteswap.la -lsnappy

couch_diff_SOURCES = src/diff.c
couch_diff_DEPENDENCIES = libcouchstore.la
couch_diff_CFLAGS = $(AM_CFLAGS) -D__STDC_FORMAT_MACROS
couch_diff_LDADD = libcouchstore.la libbyteswap.la -lsnappy

if WINDOWS
couch_dbdump_LDADD += -lws2_32
endif

couch_dbinfo_SOURCES = src/dbinfo.c src/util.c
couch_dbinfo_CFLAGS = $(AM_CFLAGS) -D__STDC_FORMAT_MACROS
couch_dbinfo_DEPENDENCIES = libcouchstore.la libbyteswap.la
couch_dbinfo_LDADD = libcouchstore.la libbyteswap.la

couch_compact_SOURCES = src/compactor.c
couch_compact_DEPENDENCIES = libcouchstore.la
couch_compact_CFLAGS = $(AM_CFLAGS) -D__STDC_FORMAT_MACROS
couch_compact_LDADD = libcouchstore.la libbyteswap.la -lsnappy

couch_viewgen_SOURCES = src/viewgen.c
couch_viewgen_DEPENDENCIES = libcouchstore.la
couch_viewgen_CFLAGS = $(AM_CFLAGS) -D__STDC_FORMAT_MACROS
couch_viewgen_LDADD = libcouchstore.la libbyteswap.la -lsnappy

extra_tests=
slow_tests=

if HAVE_LIBLUA
bin_PROGRAMS += couchscript

couchscript_CXXFLAGS = $(AM_CXXFLAGS) $(lua_CFLAGS)
couchscript_SOURCES = src/couchscript.cc
couchscript_LDFLAGS = $(AM_LDFLAGS) $(AM_CXX_LDFLAGS)
couchscript_LDADD = libcouchstore.la libbyteswap.la $(LTLIBLUA) -ldl
couchscript_DEPENDENCIES = libcouchstore.la libbyteswap.la
extra_tests += lua_tests python_tests

slow_tests += slow_lua_tests

lua_tests: couchscript couch_compact tests/localdoc.lua
	@echo "Running lua tests."
	@./couchscript tests/localdoc.lua
	@./couchscript tests/corrupt.lua
	@./couchscript tests/bulk.lua
	@./couchscript tests/changessincefilter.lua
	@./couchscript tests/compact.lua
	@./couchscript tests/dropdel.lua

slow_lua_tests: couchscript tests/largefile.lua tests/large.lua
	@echo "Running slow lua tests."
	@./couchscript tests/largefile.lua
	@./couchscript tests/large.lua

endif

python_pythonlibdir = $(libdir)/python
python_pythonlib_DATA = python/couchstore.py


python_tests: python/*.py
	@LD_LIBRARY_PATH=`pwd`/.libs \
		DYLD_LIBRARY_PATH=`pwd`/.libs \
		python python/couchstore_test.py
	@LD_LIBRARY_PATH=`pwd`/.libs \
		DYLD_LIBRARY_PATH=`pwd`/.libs \
                PYTHONPATH=${PYTHONPATH}:`pwd`/python \
		python tests/purge.py

#tests

CLEANFILES = test.couch
check_PROGRAMS = testapp
TESTS = ${check_PROGRAMS}

testapp_SOURCES = tests/testapp.c src/util.c tests/macros.h tests/collate_json_test.c tests/indexer_test.c
testapp_CFLAGS = $(AM_CFLAGS)
testapp_DEPENDENCIES = libcouchstore.la libbyteswap.la
testapp_LDADD = libcouchstore.la libbyteswap.la

test: check-TESTS $(extra_tests)

slow_test: $(slow_tests)

VALGRIND=valgrind
VALGRIND_OPTIONS=--tool=Memcheck --num-callers=50 \
                 --error-limit=yes --track-origins=yes --malloc-fill=55 \
                 --free-fill=aa --dsymutil=yes -q

VALGRIND_TEST=.libs/testapp

valgrind: ${check_PROGRAMS}
	@for f in $(VALGRIND_TEST); \
        do \
           echo Running Valgrind on $$f; \
           LD_LIBRARY_PATH=`pwd`/.libs \
           DYLD_LIBRARY_PATH=`pwd`/.libs \
           $(VALGRIND) $(VALGRIND_OPTIONS) --log-file=`basename $$f`.log $$f; \
        done

reformat:
	astyle --mode=c \
               --quiet \
               --style=1tbs \
               --indent=spaces=4 \
               --indent-namespaces \
               --indent-col1-comments \
               --max-instatement-indent=78 \
               --pad-oper \
               --pad-header \
               --add-brackets \
               --align-pointer=name \
               --align-reference=name \
               $(top_srcdir)/include/libcouchstore/*.h \
               $(top_srcdir)/src/*.[ch] \
               $(top_srcdir)/src/*.cc \
               $(top_srcdir)/tests/*.[ch]

if WINDOWS
install-data-hook:
	$(INSTALL) $(python_pythonlib_DATA) $(bindir)
	$(INSTALL) $(ICU_LIB_DIR)/icuuc50.dll $(bindir)
	$(INSTALL) $(ICU_LIB_DIR)/icudt50.dll $(bindir)
	$(INSTALL) $(ICU_LIB_DIR)/icuin50.dll $(bindir)
endif
