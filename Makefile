CC=gcc
CCFLAGS=-O -g -Wall -std=gnu99 -D_LARGEFILE64_SOURCE
LIBFLAGS=-O -g -Wall -lcrypto -std=gnu99 -fPIC -c -D_LARGEFILE64_SOURCE
SHELLFLAGS=-O -g -Wall -lm -lpthread -lz -lcrypto

all: clean lib shell

lib: src/ldb.c src/mz.c src/ldb.h
	@$(CC) $(LIBFLAGS) src/ldb.c src/mz.c
	@$(CC) -shared -Wl,-soname,libldb.so -o libldb.so ldb.o mz.o
	@echo Library is built

shell: src/shell.c src/command.c
	@$(CC) $(CCFLAGS) $(SHELLFLAGS) -c src/shell.c src/mz.c
	@$(CC) $(SHELLFLAGS) -o ldb ldb.o shell.o
	@echo Shell is built

static: src/ldb.c src/ldb.h src/shell.c
	@$(CC) $(CCFLAGS) $(SHELLFLAGS) -o ldb -O -g -Wall src/ldb.c src/ldb.h src/shell.c src/mz.c
	@echo Shell is built

distclean: clean

clean:
	@echo Cleaning...
	@rm -f *.o *.a ldb *.so

install:
	@cp ldb /usr/bin

test: ldb
	@test/test.sh

