CC=gcc
CCFLAGS=-O -g -Wall
SHELLFLAGS=-O -g -Wall -lm -lpthread

all: lib shell

lib: src/ldb.c src/ldb.h 
	@$(CC) $(CCFLAGS) -c src/ldb.c 
	@echo Library is built

shell: src/shell.c src/command.c 
	@$(CC) $(CFLAGS) $(SHELLFLAGS) -c src/shell.c 
	@$(CC) -o ldb ldb.o shell.o
	@echo Shell is built

static: src/ldb.c src/ldb.h src/shell.c
	@$(CC) $(CFLAGS) $(SHELLFLAGS) -o ldb -O -g -Wall -lm -lpthread src/ldb.c src/ldb.h src/shell.c
	@echo Shell is built

distclean: clean

clean:
	@rm -f *.o *.a ldb

install:
	@cp ldb /usr/bin

test: ldb
	@test/test.sh

