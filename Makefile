LUAJIT_PREFIX ?= /usr/local

LUAJIT_INC ?= $(LUAJIT_PREFIX)/include
LUAJIT_LIB ?= $(LUAJIT_PREFIX)/lib

CFLAGS = -Wall -Wextra -pedantic -Werror -std=c99 -g -I $(LUAJIT_INC) -I vendor/http-parser -iquote inc
LDFLAGS = -L $(LUAJIT_LIB) -lluajit-5.1

ifeq ($(shell uname),Darwin)
LDFLAGS += -pagezero_size 10000 -image_base 100000000
endif

ifeq ($(shell uname),Linux)
CFLAGS += -pthread
LDFLAGS += -pthread
endif

OBJECTS = \
	src/buffer.o \
	src/client.o \
	src/http.o \
	src/main.o \
	src/server.o \
	src/util.o \
	vendor/http-parser/http_parser.o

.PHONY: clean

all: orka

clean:
	rm -f orka $(OBJECTS)

orka: $(OBJECTS) Makefile
	$(CC) -o $@ $(LDFLAGS) $(OBJECTS)

%.o: %.c Makefile inc/*.h
	$(CC) -o $@ $(CFLAGS) -c $<
