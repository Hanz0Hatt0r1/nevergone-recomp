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
last_cid = nil
created_name = nil
created_career = nil
g_UILogin = {}
function g_UILogin.EnterGameLogicServer(ip, id)
    assert(type(ip) == 'string')
    assert(type(id) == 'number')
    last_ip = ip
    last_id = id
end
function g_UILogin.EnterGameWithCid(cid)
    assert(type(cid) == 'number')
    last_cid = cid
end
function g_UILogin.CreateCharacter(name, career)
    assert(type(name) == 'string')
    assert(type(career) == 'number')
    created_name = name
    created_career = career
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

    assert(nevergone::login_lua_dispatch::call_enter_game_with_cid(
        state, 202, &error));
    assert(error.empty());
    lua_getglobal(state, "last_cid");
    assert(lua_tointeger(state, -1) == 202);
    lua_pop(state, 1);

    assert(nevergone::login_lua_dispatch::call_create_character(
        state, "Dara", 2, &error));
    assert(error.empty());
    lua_getglobal(state, "created_name");
    assert(std::string(lua_tostring(state, -1)) == "Dara");
    lua_pop(state, 1);
    lua_getglobal(state, "created_career");
    assert(lua_tointeger(state, -1) == 2);
    lua_pop(state, 1);

    assert(!nevergone::login_lua_dispatch::call_enter_game_logic_server(
        state, "", 7, &error));
    assert(error == "server IP is empty");
    assert(!nevergone::login_lua_dispatch::call_create_character(
        state, "", 1, &error));
    assert(error == "character name is empty");

    lua_pushnil(state);
    lua_setglobal(state, "g_UILogin");
    assert(!nevergone::login_lua_dispatch::call_enter_game_logic_server(
        state, "10.0.0.1", 8, &error));
    assert(error == "g_UILogin table unavailable");
    assert(!nevergone::login_lua_dispatch::call_enter_game_with_cid(
        state, 303, &error));
    assert(error == "g_UILogin table unavailable");

    assert(luaL_dostring(state, "g_UILogin = {}") == 0);
    assert(!nevergone::login_lua_dispatch::call_enter_game_logic_server(
        state, "10.0.0.2", 9, &error));
    assert(error == "g_UILogin.EnterGameLogicServer unavailable");
    assert(!nevergone::login_lua_dispatch::call_enter_game_with_cid(
        state, 404, &error));
    assert(error == "g_UILogin.EnterGameWithCid unavailable");
    assert(!nevergone::login_lua_dispatch::call_create_character(
        state, "Eris", 3, &error));
    assert(error == "g_UILogin.CreateCharacter unavailable");

    assert(luaL_dostring(state,
        "g_UILogin.EnterGameLogicServer = function() error('fixture failure') end\n"
        "g_UILogin.EnterGameWithCid = function() error('cid failure') end\n"
        "g_UILogin.CreateCharacter = function() error('create failure') end") == 0);
    assert(!nevergone::login_lua_dispatch::call_enter_game_logic_server(
        state, "10.0.0.3", 10, &error));
    assert(error.find("fixture failure") != std::string::npos);
    assert(!nevergone::login_lua_dispatch::call_enter_game_with_cid(
        state, 505, &error));
    assert(error.find("cid failure") != std::string::npos);
    assert(!nevergone::login_lua_dispatch::call_create_character(
        state, "Faye", 4, &error));
    assert(error.find("create failure") != std::string::npos);

    lua_close(state);
    std::cout << "login Lua dispatch smoke: ok\n";
    return 0;
}