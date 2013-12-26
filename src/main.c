#include <stdlib.h>
#include <stdio.h>
#include <luajit-2.0/lua.h>
#include <luajit-2.0/lauxlib.h>
#include <luajit-2.0/lualib.h>
#include <luajit-2.0/luajit.h>

lua_State*
orka_lua;

uv_loop_t*
orka_loop;

static void
init_lua()
{
    orka_lua = lua_open();

    if(!orka_lua) {
        fprintf(stderr, "Could not create LuaJIT state\n");
        exit(1);
    }

    luaL_openlibs(orka_lua);
}

static void
init_uv()
{
    orka_loop = uv_loop_new();
    if(!orka_loop) {
        fprintf(stderr, "Could not create libuv loop");
        exit(1);
    }
}

int
main(int argc, char** argv)
{
    if(argc < 2) {
        fprintf(stderr, "Usage: orka <config file>\n");
        exit(1);
    }

    init_lua();
    init_uv();

    if(luaL_dofile(orka_lua, argv[1])) {
        fprintf(stderr, "%s\n", lua_tostring(orka_lua, -1));
        exit(1);
    }

    return 0;
}
