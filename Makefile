ifeq ($(origin CC),default)
CC = gcc
endif
CCFLAGS ?= -O -lz -Wall -Wno-unused-result -Wno-deprecated-declarations -g  -D_LARGEFILE64_SOURCE -D_GNU_SOURCE -fPIC -Wno-format-truncation -I./src/ldb
LDFLAGS+= -lm -lpthread -lz -ldl -lgcrypt
SOURCES=$(wildcard src/*.c)
OBJECTS=$(SOURCES:.c=.o) 
TARGET=ldb
LIB=libldb.so
LOGDIR:=/var/log/scanoss/ldb/
$(TARGET): $(OBJECTS)
	$(CC) -g -o $(TARGET) $^ $(LDFLAGS)

VERSION=$(shell ./version.sh)

all: clean $(TARGET) lib

lib:  $(OBJECTS)
	$(CC) -g -o $(LIB) $^ $(LDFLAGS)  -shared -Wl,-soname,$(LIB)
.PHONY: ldb

%.o: %.c
	$(CC) $(CCFLAGS) -o $@ -c $<

clean_build:
	rm -rf src/*.o src/**/*.o external/src/*.o external/src/**/*.o

clean: clean_build
	rm -rf $(TARGET)
	rm -rf $(LIB)
distclean: clean

install: $(TARGET) lib
	@cp $(TARGET) /usr/bin
	@cp $(LIB) /usr/lib
	@cp -r src/ldb /usr/include
	@cp src/ldb.h /usr/include
	@mkdir -p $(LOGDIR) && chown -R $(SUDO_USER) $(LOGDIR) && chmod -R u+rw $(LOGDIR)
uninstall:
	@rm -r /usr/include/ldb
	@rm /usr/include/ldb.h
	@rm /usr/lib/libldb.so
prepare_deb_package: all ## Prepares the deb Package 
	@./package.sh deb $(VERSION)
	@echo deb package built

prepare_rpm_package: all ## Prepares the rpm Package 
	@./package.sh rpm $(VERSION)
	@echo rpm package built