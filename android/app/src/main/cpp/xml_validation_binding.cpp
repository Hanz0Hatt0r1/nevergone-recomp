#include "xml_validation_binding.h"

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}
#endif

namespace nevergone::lua_runtime {

#if defined(NEVERGONE_HAS_LUA)
namespace {

int l_Lua_IsXmlValid(lua_State* state) {
    const char* path = luaL_checkstring(state, 1);
    lua_getglobal(state, "xml");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_pushboolean(state, 0);
        return 1;
    }

    lua_getfield(state, -1, "load");
    if (!lua_isfunction(state, -1)) {
        lua_pop(state, 2);
        lua_pushboolean(state, 0);
        return 1;
    }

    lua_pushstring(state, path);
    const int status = lua_pcall(state, 1, 1, 0);
    const bool valid = status == 0 && lua_istable(state, -1);
    lua_settop(state, 0);
    lua_pushboolean(state, valid ? 1 : 0);
    return 1;
}

}  // namespace
#endif

void register_xml_validation_binding(lua_State* state) {
#if defined(NEVERGONE_HAS_LUA)
    lua_pushcfunction(state, l_Lua_IsXmlValid);
    lua_setglobal(state, "Lua_IsXmlValid");
#else
    (void)state;
#endif
}

}  // namespace nevergone::lua_runtime
