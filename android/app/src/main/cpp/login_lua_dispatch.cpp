#include "login_lua_dispatch.h"

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lua.h>
#include <lauxlib.h>
}
#endif

namespace nevergone::login_lua_dispatch {
#if defined(NEVERGONE_HAS_LUA)
namespace {

bool push_login_function(lua_State* state, const char* function_name, std::string* error) {
    lua_getglobal(state, "g_UILogin");
    if (!lua_istable(state, -1)) {
        if (error != nullptr) *error = "g_UILogin table unavailable";
        return false;
    }
    lua_getfield(state, -1, function_name);
    if (!lua_isfunction(state, -1)) {
        if (error != nullptr) {
            *error = std::string("g_UILogin.") + function_name + " unavailable";
        }
        return false;
    }
    lua_remove(state, -2);  // recovered signatures do not pass the table as self
    return true;
}

bool finish_call(
        lua_State* state,
        int stack_base,
        int argument_count,
        const char* fallback_error,
        std::string* error) {
    const int status = lua_pcall(state, argument_count, 0, 0);
    if (status != 0) {
        const char* message = lua_tostring(state, -1);
        if (error != nullptr) *error = message != nullptr ? message : fallback_error;
        lua_settop(state, stack_base);
        return false;
    }
    lua_settop(state, stack_base);
    if (error != nullptr) error->clear();
    return true;
}

}  // namespace
#endif

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
    if (!push_login_function(state, "EnterGameLogicServer", error)) {
        lua_settop(state, stack_base);
        return false;
    }
    lua_pushlstring(state, ip.data(), ip.size());
    lua_pushinteger(state, static_cast<lua_Integer>(server_id));
    return finish_call(state, stack_base, 2, "EnterGameLogicServer failed", error);
#else
    (void)state;
    (void)ip;
    (void)server_id;
    if (error != nullptr) *error = "Lua runtime unavailable";
    return false;
#endif
}

bool call_enter_game_with_cid(
    lua_State* state,
    std::int64_t character_id,
    std::string* error) {
#if defined(NEVERGONE_HAS_LUA)
    if (state == nullptr) {
        if (error != nullptr) *error = "Lua state unavailable";
        return false;
    }
    const int stack_base = lua_gettop(state);
    if (!push_login_function(state, "EnterGameWithCid", error)) {
        lua_settop(state, stack_base);
        return false;
    }
    lua_pushinteger(state, static_cast<lua_Integer>(character_id));
    return finish_call(state, stack_base, 1, "EnterGameWithCid failed", error);
#else
    (void)state;
    (void)character_id;
    if (error != nullptr) *error = "Lua runtime unavailable";
    return false;
#endif
}

bool call_create_character(
    lua_State* state,
    const std::string& character_name,
    std::int64_t career,
    std::string* error) {
#if defined(NEVERGONE_HAS_LUA)
    if (state == nullptr) {
        if (error != nullptr) *error = "Lua state unavailable";
        return false;
    }
    if (character_name.empty()) {
        if (error != nullptr) *error = "character name is empty";
        return false;
    }
    const int stack_base = lua_gettop(state);
    if (!push_login_function(state, "CreateCharacter", error)) {
        lua_settop(state, stack_base);
        return false;
    }
    lua_pushlstring(state, character_name.data(), character_name.size());
    lua_pushinteger(state, static_cast<lua_Integer>(career));
    return finish_call(state, stack_base, 2, "CreateCharacter failed", error);
#else
    (void)state;
    (void)character_name;
    (void)career;
    if (error != nullptr) *error = "Lua runtime unavailable";
    return false;
#endif
}

}  // namespace nevergone::login_lua_dispatch