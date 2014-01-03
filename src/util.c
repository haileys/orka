#include <errno.h>
#include <string.h>

#include "orka.h"

int
orka_error(lua_State* l, const char* function_name)
{
    const char* err = strerror(errno);
    return luaL_error(l, "%s: %s", function_name, err);
}
