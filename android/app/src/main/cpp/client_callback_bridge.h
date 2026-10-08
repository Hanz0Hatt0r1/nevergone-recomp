#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct lua_State;

namespace nevergone::lua_runtime {

struct ClientCallbackEvent {
    std::string name;
    std::vector<std::string> arguments;
};

struct ClientUiSnapshot {
    std::size_t event_count = 0;
    std::string last_event;
    std::string server_list;
    std::string role_list;
    std::string created_role;
    std::string announcement;
    std::string enter_game;
    std::string chat_messages;
    std::string update_data;
    std::string pve_connect;
};

void register_login_callback_bindings(lua_State* state);
std::vector<ClientCallbackEvent> take_client_callback_events();
ClientUiSnapshot snapshot_client_ui_state();
void reset_client_ui_state();
std::string client_ui_state_report();

}  // namespace nevergone::lua_runtime
