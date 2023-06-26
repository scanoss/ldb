ifeq ($(origin CC),default)
CC=gcc
endif
CCFLAGS?=-O -g -Wall -std=gnu99
LIBFLAGS=$(CCFLAGS) -fPIC -c
LIBS=-lm -lpthread -lz 

VERSION=$(shell ./version.sh)

# HELP
# This will output the help for each task
# thanks to https://marmelab.com/blog/2016/02/29/auto-documented-makefile.html
.PHONY: help

help: ## This help
	@awk 'BEGIN {FS = ":.*?## "} /^[0-9a-zA-Z_-]+:.*?## / {printf "\033[36m%-20s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)

all: clean lib shell ## Clean dev data and build the ldb's library and shell binaries

lib: src/ldb.c src/mz.c src/ldb.h ## Build only the ldb library
	@$(CC) $(LIBFLAGS) -D_LARGEFILE64_SOURCE src/ldb.c src/mz.c $(LIBS)
	@$(CC) -shared -Wl,-soname,libldb.so -o libldb.so ldb.o mz.o $(LIBS)
	@echo Library is built

shell: src/shell.c src/command.c ## Build only the shell binary
	@$(CC) $(CCFLAGS) -D_LARGEFILE64_SOURCE -c src/shell.c src/mz.c $(LIBS)
	@$(CC) $(CCFLAGS) -o ldb ldb.o shell.o -lcrypto $(LIBS)
	@echo Shell is built

static: src/ldb.c src/ldb.h src/shell.c ## Static build of the shell binary
	@$(CC) $(CCFLAGS) -o ldb src/ldb.c src/ldb.h src/shell.c src/mz.c $(LIBS)
	@echo Shell is built

distclean: clean 

clean:  ## Cleans dev data
	@echo Cleaning...
	@rm -f *.o *.a ldb *.so
	@rm -rf dist

install:  ## Install the library and shell
	@cp ldb /usr/bin
	@cp libldb.so /usr/lib
	@cp src/ldb.h /usr/include

test: ldb ## Run tests
	@test/test.sh

prepare_deb_package: all ## Prepares the deb Package 
	@./package.sh deb $(VERSION)
	@echo deb package built

prepare_rpm_package: all ## Prepares the rpm Package 
	@./package.sh rpm $(VERSION)
	@echo rpm package built