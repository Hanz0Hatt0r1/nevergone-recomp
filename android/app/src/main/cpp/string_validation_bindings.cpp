#include "string_validation_bindings.h"

#include <string>

#include "string_validation.h"

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}
#endif

namespace nevergone::lua_runtime {

#if defined(NEVERGONE_HAS_LUA)
namespace {

int l_Lua_CheckNickName(lua_State* state) {
    size_t length = 0;
    const char* value = luaL_checklstring(state, 1, &length);
    lua_pushboolean(
        state,
        nevergone::string_validation::nickname_is_valid(std::string(value, length)) ? 1 : 0);
    return 1;
}

int l_Lua_CheckStringLegal(lua_State* state) {
    size_t length = 0;
    const char* value = luaL_checklstring(state, 1, &length);
    lua_pushboolean(
        state,
        nevergone::string_validation::string_is_legal(std::string(value, length)) ? 1 : 0);
    return 1;
}

}  // namespace
#endif

void register_string_validation_bindings(lua_State* state) {
#if defined(NEVERGONE_HAS_LUA)
    lua_pushcfunction(state, l_Lua_CheckNickName);
    lua_setglobal(state, "Lua_CheckNickName");

    lua_pushcfunction(state, l_Lua_CheckStringLegal);
    lua_setglobal(state, "Lua_CheckStringLegal");
#else
    (void)state;
#endif
}

}  // namespace nevergone::lua_runtime
