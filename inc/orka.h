#ifndef ORKA_H
#define ORKA_H

#include <http_parser.h>
#include <luajit-2.0/lua.h>
#include <luajit-2.0/lauxlib.h>
#include <stdbool.h>

#include "buffer.h"

extern struct lua_State*
orka_lua;

void
orka_gil_acquire();

void
orka_gil_release();

#define ORKA_WITH_GIL(block) do { \
    orka_gil_acquire(); \
    block; \
    orka_gil_release(); \
} while(0)

int
orka_error(lua_State* l, const char* function_name);

const char*
orka_status_code(int status);

#endif
