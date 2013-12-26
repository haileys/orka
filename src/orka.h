#ifndef ORKA_H
#define ORKA_H

#include <uv.h>
#include <luajit-2.0/lua.h>

extern lua_State*
orka_lua;

extern uv_loop_t*
orka_loop;

#endif
