#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nevergone::login_callback_payload {

struct ServerEntry {
    std::int64_t id = 0;
    std::string name;
    std::string ip;
    std::string battle_ip;
};

struct ServerListPayload {
    bool valid = false;
    std::vector<ServerEntry> servers;
    std::string last_login_server;
};

struct RoleEntry {
    std::int64_t character_id = 0;
    std::string character_name;
    std::int64_t career = 0;
    std::int64_t character_level = 0;
    std::int64_t clothes_id = 0;
    std::int64_t clothes_color_id = 0;
};

struct RoleListPayload {
    bool valid = false;
    std::vector<RoleEntry> roles;
};

bool parse_server_list_callback(
    const std::vector<std::string>& arguments,
    ServerListPayload* output);

bool parse_role_list_callback(
    const std::vector<std::string>& arguments,
    RoleListPayload* output);

}  // namespace nevergone::login_callback_payload
