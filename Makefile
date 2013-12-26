LIBUV_PREFIX ?= /usr/local
LUAJIT_PREFIX ?= /usr/local

LIBUV_INC ?= $(LIBUV_PREFIX)/include
LIBUV_LIB ?= $(LIBUV_PREFIX)/lib

LUAJIT_INC ?= $(LUAJIT_PREFIX)/include
LUAJIT_LIB ?= $(LUAJIT_PREFIX)/lib

CFLAGS = -Wall -Wextra -pedantic -Werror -std=c99 -g -I $(LIBUV_INC) -I $(LUAJIT_INC)
LDFLAGS = -L $(LIBUV_LIB) -L $(LUAJIT_LIB) -luv -lluajit-5.1

OBJECTS = src/main.o

.PHONY: clean

all: orka

clean:
	rm -f orka $(OBJECTS)

orka: $(OBJECTS) Makefile
	$(CC) -o $@ $(LDFLAGS) $(OBJECTS)

%.o: %.c Makefile
	$(CC) -o $@ $(CFLAGS) -c $<
