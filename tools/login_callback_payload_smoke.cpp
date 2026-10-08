#include <cassert>
#include <string>
#include <vector>

#include "login_callback_payload.h"

int main() {
    using namespace nevergone::login_callback_payload;

    ServerListPayload servers;
    const std::vector<std::string> server_arguments = {
        R"([{"ip":"10.0.0.1","id":7,"name":"Europe","BattleIP":"10.0.0.2","ignored":{"load":2}},{"id":8,"name":"Asia \u2605","ip":"10.0.1.1"}])",
        "8",
    };
    // The fixture above mirrors a Lua cjson string; remove C++-level escaping
    // before handing it to the parser so this smoke also checks exact JSON.
    std::string server_json = server_arguments[0];
    for (std::size_t pos = 0; (pos = server_json.find("\\\"", pos)) != std::string::npos;) {
        server_json.replace(pos, 2, "\"");
        ++pos;
    }
    std::vector<std::string> normalized_server_arguments = {server_json, "8"};
    assert(parse_server_list_callback(normalized_server_arguments, &servers));
    assert(servers.valid);
    assert(servers.servers.size() == 2);
    assert(servers.servers[0].id == 7);
    assert(servers.servers[0].name == "Europe");
    assert(servers.servers[0].ip == "10.0.0.1");
    assert(servers.servers[0].battle_ip == "10.0.0.2");
    assert(servers.servers[1].id == 8);
    assert(servers.servers[1].name == "Asia \xE2\x98\x85");
    assert(servers.last_login_server == "8");

    ServerListPayload invalid_servers;
    assert(!parse_server_list_callback({"{not-json}"}, &invalid_servers));
    assert(!invalid_servers.valid);

    RoleListPayload roles;
    const std::vector<std::string> role_arguments = {
        R"({"CidList":[101,202],"CharacterDataMap":{"101":{"CharacterID":101,"CharacterName":"Aria","Career":1,"CharacterLevel":12,"ClothesID":31,"ClothesColorID":4},"202":{"CharacterID":202,"CharacterName":"Bram","Career":2,"CharacterLevel":7,"ClothesID":19,"ClothesColorID":3}},"NestedEcho":{"CharacterID":101,"CharacterName":"Aria","Career":1}})"
    };
    std::string role_json = role_arguments[0];
    for (std::size_t pos = 0; (pos = role_json.find("\\\"", pos)) != std::string::npos;) {
        role_json.replace(pos, 2, "\"");
        ++pos;
    }
    assert(parse_role_list_callback({role_json}, &roles));
    assert(roles.valid);
    assert(roles.roles.size() == 2);
    assert(roles.roles[0].character_id == 101);
    assert(roles.roles[0].character_name == "Aria");
    assert(roles.roles[0].career == 1);
    assert(roles.roles[0].character_level == 12);
    assert(roles.roles[0].clothes_id == 31);
    assert(roles.roles[0].clothes_color_id == 4);
    assert(roles.roles[1].character_id == 202);
    assert(roles.roles[1].character_name == "Bram");

    RoleListPayload empty_roles;
    assert(parse_role_list_callback({R"({"CidList":[],"CharacterDataMap":{}})"}, &empty_roles) == false);
    const std::string empty_role_json = R"({"CidList":[],"CharacterDataMap":{}})";
    std::string normalized_empty_role_json = empty_role_json;
    for (std::size_t pos = 0; (pos = normalized_empty_role_json.find("\\\"", pos)) != std::string::npos;) {
        normalized_empty_role_json.replace(pos, 2, "\"");
        ++pos;
    }
    assert(parse_role_list_callback({normalized_empty_role_json}, &empty_roles));
    assert(empty_roles.valid);
    assert(empty_roles.roles.empty());

    return 0;
}
