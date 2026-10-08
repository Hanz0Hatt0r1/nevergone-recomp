#pragma once

#include <cstdint>
#include <string>

struct lua_State;

namespace nevergone::login_lua_dispatch {

// Reconstructed semantic call behind LUA_LOGIN::lua_CallGameRPC(ip, id):
// g_UILogin.EnterGameLogicServer(ip, id).
bool call_enter_game_logic_server(
    lua_State* state,
    const std::string& ip,
    std::int64_t server_id,
    std::string* error);

}  // namespace nevergone::login_lua_dispatch
