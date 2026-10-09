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

// Reconstructed semantic call behind online character start:
// g_UILogin.EnterGameWithCid(CharacterID).
bool call_enter_game_with_cid(
    lua_State* state,
    std::int64_t character_id,
    std::string* error);

// Reconstructed semantic call behind online role creation:
// g_UILogin.CreateCharacter(name, career).
bool call_create_character(
    lua_State* state,
    const std::string& character_name,
    std::int64_t career,
    std::string* error);

}  // namespace nevergone::login_lua_dispatch
