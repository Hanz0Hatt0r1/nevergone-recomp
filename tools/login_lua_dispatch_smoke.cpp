#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "login_lua_dispatch.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

int main() {
    lua_State* state = luaL_newstate();
    assert(state != nullptr);
    luaL_openlibs(state);

    const char* fixture = R"LUA(
last_ip = nil
last_id = nil
g_UILogin = {}
function g_UILogin.EnterGameLogicServer(ip, id)
    assert(type(ip) == 'string')
    assert(type(id) == 'number')
    last_ip = ip
    last_id = id
end
)LUA";
    assert(luaL_dostring(state, fixture) == 0);

    std::string error;
    assert(nevergone::login_lua_dispatch::call_enter_game_logic_server(
        state, "127.0.0.1:9001", 42, &error));
    assert(error.empty());

    lua_getglobal(state, "last_ip");
    assert(std::string(lua_tostring(state, -1)) == "127.0.0.1:9001");
    lua_pop(state, 1);
    lua_getglobal(state, "last_id");
    assert(lua_tointeger(state, -1) == 42);
    lua_pop(state, 1);

    assert(!nevergone::login_lua_dispatch::call_enter_game_logic_server(
        state, "", 7, &error));
    assert(error == "server IP is empty");

    lua_pushnil(state);
    lua_setglobal(state, "g_UILogin");
    assert(!nevergone::login_lua_dispatch::call_enter_game_logic_server(
        state, "10.0.0.1", 8, &error));
    assert(error == "g_UILogin table unavailable");

    assert(luaL_dostring(state, "g_UILogin = {}") == 0);
    assert(!nevergone::login_lua_dispatch::call_enter_game_logic_server(
        state, "10.0.0.2", 9, &error));
    assert(error == "g_UILogin.EnterGameLogicServer unavailable");

    assert(luaL_dostring(state,
        "g_UILogin.EnterGameLogicServer = function() error('fixture failure') end") == 0);
    assert(!nevergone::login_lua_dispatch::call_enter_game_logic_server(
        state, "10.0.0.3", 10, &error));
    assert(error.find("fixture failure") != std::string::npos);

    lua_close(state);
    std::cout << "login Lua dispatch smoke: ok\n";
    return 0;
}
