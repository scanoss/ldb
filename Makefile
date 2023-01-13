ifeq ($(origin CC),default)
CC=gcc
endif

CCFLAGS?=-O -g -Wall -std=gnu99
LIBFLAGS=$(CCFLAGS) -fPIC -c
LIBS=-lm -lpthread -lz

ifneq ("$(EXTERN_LIB)","")
CCFLAGS+=-DLDB_EXTRACT_DEFINED_EXTERN
LIBS+=-l$(EXTERN_LIB)
endif

all: clean lib shell

lib: src/ldb.c src/mz.c src/ldb.h
	@echo $(LIBS)
	@echo $(CCFLAGS)
	@$(CC) $(LIBFLAGS) -D_LARGEFILE64_SOURCE src/ldb.c src/mz.c $(LIBS)
	@$(CC) -shared -Wl,-soname,libldb.so -o libldb.so ldb.o mz.o $(LIBS)
	@echo Library is built

shell: src/shell.c src/command.c
	@$(CC) $(CCFLAGS) -D_LARGEFILE64_SOURCE -c src/shell.c src/mz.c $(LIBS)
	@$(CC) $(CCFLAGS) -o ldb ldb.o shell.o -lcrypto $(LIBS)
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

