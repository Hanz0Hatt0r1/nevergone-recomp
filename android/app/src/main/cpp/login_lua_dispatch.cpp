#include "login_lua_dispatch.h"

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#endif

namespace nevergone::login_lua_dispatch {

bool call_enter_game_logic_server(
    lua_State* state,
    const std::string& ip,
    std::int64_t server_id,
    std::string* error) {
#if defined(NEVERGONE_HAS_LUA)
    if (state == nullptr) {
        if (error != nullptr) *error = "Lua state unavailable";
        return false;
    }
    if (ip.empty()) {
        if (error != nullptr) *error = "server IP is empty";
        return false;
    }

    const int stack_base = lua_gettop(state);
    lua_getglobal(state, "g_UILogin");
    if (!lua_istable(state, -1)) {
        lua_settop(state, stack_base);
        if (error != nullptr) *error = "g_UILogin table unavailable";
        return false;
    }

    lua_getfield(state, -1, "EnterGameLogicServer");
    if (!lua_isfunction(state, -1)) {
        lua_settop(state, stack_base);
        if (error != nullptr) *error = "g_UILogin.EnterGameLogicServer unavailable";
        return false;
    }

    lua_remove(state, -2);  // remove table; recovered signature is s,i (no self)
    lua_pushlstring(state, ip.data(), ip.size());
    lua_pushinteger(state, static_cast<lua_Integer>(server_id));
    const int status = lua_pcall(state, 2, 0, 0);
    if (status != LUA_OK) {
        const char* message = lua_tostring(state, -1);
        if (error != nullptr) {
            *error = message != nullptr ? message : "EnterGameLogicServer failed";
        }
        lua_settop(state, stack_base);
        return false;
    }

    lua_settop(state, stack_base);
    if (error != nullptr) error->clear();
    return true;
#else
    (void)state;
    (void)ip;
    (void)server_id;
    if (error != nullptr) *error = "Lua runtime unavailable";
    return false;
#endif
}

}  // namespace nevergone::login_lua_dispatch
