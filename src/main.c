#include <luajit-2.0/lauxlib.h>
#include <luajit-2.0/lualib.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "orka.h"

lua_State*
orka_lua;

static pthread_mutex_t
gil = PTHREAD_MUTEX_INITIALIZER;

void
orka_gil_acquire()
{
    pthread_mutex_lock(&gil);
}

void
orka_gil_release()
{
    pthread_mutex_unlock(&gil);
}

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
init_orka_libs()
{
    #define INIT(lib) \
        void orka_init_##lib##_lib(); \
        orka_init_##lib##_lib();

    INIT(http);
}

int
main(int argc, char** argv)
{
    if(argc < 2) {
        fprintf(stderr, "Usage: orka <config file>\n");
        exit(1);
    }

    init_lua();
    init_orka_libs();

    if(luaL_dofile(orka_lua, argv[1])) {
        fprintf(stderr, "%s\n", lua_tostring(orka_lua, -1));
        exit(1);
    }

    while(1) {
        pause();
    }

    return 0;
}
