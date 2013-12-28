#include "orka.h"

int
orka_uv_error(lua_State* l, const char* function_name)
{
    const char* err = uv_strerror(uv_last_error(orka_loop));
    return luaL_error(l, "%s: %s", function_name, err);
}
