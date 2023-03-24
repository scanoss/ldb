ifeq ($(origin CC),default)
CC=gcc
endif
CCFLAGS?=-O -g -Wall -std=gnu99 -D_GNU_SOURCE -Wno-format-truncation
LIBFLAGS=$(CCFLAGS) -fPIC -c -ldl
LIBS=-lm -lpthread -lz -ldl
LDB_CONF_PATH=/usr/local/etc/scanoss/ldb/

all: clean lib shell

lib: src/ldb.c src/mz.c src/ldb.h
	@$(CC) $(LIBFLAGS) -D_LARGEFILE64_SOURCE src/ldb.c src/mz.c $(LIBS)
	@$(CC) -shared -Wl,-soname,libldb.so -o libldb.so ldb.o mz.o $(LIBS)
	@echo Library is built

shell: src/shell.c src/command.c
	@$(CC) $(CCFLAGS) -D_LARGEFILE64_SOURCE -c src/shell.c src/mz.c src/import.c src/join.c src/bsort.c $(LIBS)
	@$(CC) $(CCFLAGS) -o ldb ldb.o shell.o import.o join.o bsort.o -lcrypto $(LIBS)
	@echo Shell is built

static: src/ldb.c src/ldb.h src/shell.c
	@$(CC) $(CCFLAGS) -o ldb src/ldb.c src/ldb.h src/shell.c src/mz.c $(LIBS)
	@echo Shell is built

distclean: clean

clean:
	@echo Cleaning...
	@rm -f *.o *.a ldb *.so

install:
	@cp ldb /usr/bin
	@cp libldb.so /usr/lib
	@cp src/ldb.h /usr/include

test: ldb
	@test/test.sh

config:
	@mkdir -p $(LDB_CONF_PATH)
	@echo "GLOBAL: (COLLATE_MAX_RECORD=2048, TMP_PATH=/home/scanoss)" > $(LDB_CONF_PATH)/oss.conf
	@echo "file: (KEY2=1, FIELDS=3)" >> $(LDB_CONF_PATH)/oss.conf
	@echo "url: (FIELDS=8)" >> $(LDB_CONF_PATH)/oss.conf
	@echo "purl: (SKIP_FIELDS_CHECK=1)" >> $(LDB_CONF_PATH)/oss.conf
	@echo "license: (FIELDS=3)" >> $(LDB_CONF_PATH)/oss.conf
	@echo "dependency: (FIELDS=5)" >> $(LDB_CONF_PATH)/oss.conf
	@echo "copyright: (FIELDS=3)" >> $(LDB_CONF_PATH)/oss.conf
	@echo "vulnerability: (FIELDS=10)" >> $(LDB_CONF_PATH)/ldb.conf
	@echo "quality: (FIELDS=3)" >> $(LDB_CONF_PATH)/oss.conf
	@echo "cryptography: (FIELDS=3)" >> $(LDB_CONF_PATH)/oss.conf
	@echo "attribution: (FIELDS=2)" >> $(LDB_CONF_PATH)/oss.conf
	@echo "wfp: (WFP=1)" >> $(LDB_CONF_PATH)/oss.conf
	@echo "sources: (MZ=1)" >> $(LDB_CONF_PATH)/oss.conf
	@echo "notices: (MZ=1)" >> $(LDB_CONF_PATH)/oss.conf
	@echo "pivot: (KEY2=1, FIELDS=1, SKIP_FIELDS_CHECK=1)" >> $(LDB_CONF_PATH)/oss.conf