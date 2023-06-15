ifeq ($(origin CC),default)
CC = gcc
endif
CCFLAGS ?= -O -lz -Wall -Wno-unused-result -Wno-deprecated-declarations -g  -D_LARGEFILE64_SOURCE -D_GNU_SOURCE -fPIC -Wno-format-truncation
LDFLAGS+= -lm -lpthread -lz -ldl -lcrypto
SOURCES=$(wildcard src/*.c)
OBJECTS=$(SOURCES:.c=.o) 
TARGET=ldb
LIB=libldb.so
$(TARGET): $(OBJECTS)
	$(CC) -g -o $(TARGET) $^ $(LDFLAGS)

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

install:
	@cp $(TARGET) /usr/bin
	@cp $(LIB) /usr/lib

