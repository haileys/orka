#include <stdlib.h>
#include <stdio.h>
#include <luajit-2.0/lua.h>
#include <luajit-2.0/lauxlib.h>
#include <luajit-2.0/lualib.h>
#include <luajit-2.0/luajit.h>

static lua_State*
init_lua()
{
    lua_State* lua = lua_open();

    if(!lua) {
        fprintf(stderr, "Could not create LuaJIT state\n");
        exit(1);
    }

    luaL_openlibs(lua);

    return lua;
}

int
main(int argc, char** argv)
{
    if(argc < 2) {
        fprintf(stderr, "Usage: orka <config file>\n");
        exit(1);
    }

    lua_State* lua = init_lua();

    if(luaL_dofile(lua, argv[1])) {
        fprintf(stderr, "%s\n", lua_tostring(lua, -1));
        exit(1);
    }

    lua_close(lua);
    return 0;
}
