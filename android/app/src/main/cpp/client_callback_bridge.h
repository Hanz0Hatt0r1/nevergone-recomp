#pragma once

#include <string>
#include <vector>

struct lua_State;

namespace nevergone::lua_runtime {

struct ClientCallbackEvent {
    std::string name;
    std::vector<std::string> arguments;
};

void register_login_callback_bindings(lua_State* state);
std::vector<ClientCallbackEvent> take_client_callback_events();

}  // namespace nevergone::lua_runtime
