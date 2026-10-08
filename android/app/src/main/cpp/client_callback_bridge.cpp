#include "client_callback_bridge.h"

#include <mutex>
#include <sstream>
#include <utility>

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lua.h>
}
#endif

namespace nevergone::lua_runtime {
namespace {

std::mutex g_callback_mutex;
std::vector<ClientCallbackEvent> g_callback_events;

#if defined(NEVERGONE_HAS_LUA)
std::string argument_to_string(lua_State* state, int index) {
    switch (lua_type(state, index)) {
        case LUA_TNIL:
            return "nil";
        case LUA_TBOOLEAN:
            return lua_toboolean(state, index) != 0 ? "true" : "false";
        case LUA_TNUMBER: {
            std::ostringstream out;
            out << lua_tonumber(state, index);
            return out.str();
        }
        case LUA_TSTRING: {
            size_t length = 0;
            const char* value = lua_tolstring(state, index, &length);
            return value != nullptr ? std::string(value, length) : std::string();
        }
        default:
            return std::string("<") + lua_typename(state, lua_type(state, index)) + ">";
    }
}

int l_capture_callback(lua_State* state) {
    const char* name = lua_tostring(state, lua_upvalueindex(1));
    ClientCallbackEvent event;
    event.name = name != nullptr ? name : "unknown";

    const int count = lua_gettop(state);
    event.arguments.reserve(static_cast<size_t>(count));
    for (int index = 1; index <= count; ++index) {
        event.arguments.push_back(argument_to_string(state, index));
    }

    std::lock_guard<std::mutex> lock(g_callback_mutex);
    g_callback_events.push_back(std::move(event));
    return 0;
}

void register_callback(lua_State* state, const char* name) {
    lua_pushstring(state, name);
    lua_pushcclosure(state, l_capture_callback, 1);
    lua_setglobal(state, name);
}
#endif

}  // namespace

void register_login_callback_bindings(lua_State* state) {
#if defined(NEVERGONE_HAS_LUA)
    {
        std::lock_guard<std::mutex> lock(g_callback_mutex);
        g_callback_events.clear();
    }
    register_callback(state, "cpp_OnGetServerList");
    register_callback(state, "cpp_OnGetRoleList");
    register_callback(state, "cpp_OnCreateTheRole");
    register_callback(state, "cpp_OnGameAnnoucement");
    register_callback(state, "cpp_OnEnterGame");

    // These reachable callbacks are also backed by exported native symbols in
    // the original ARMv7 client. Keep them on the same typed event bridge so
    // Lua can advance without pretending that chat/data/PVE UI consumers have
    // already been reconstructed.
    register_callback(state, "cpp_OnReceivedChatMessages");
    register_callback(state, "cpp_OnUpdateData");
    register_callback(state, "cpp_connect_pve");
#else
    (void)state;
#endif
}

std::vector<ClientCallbackEvent> take_client_callback_events() {
    std::lock_guard<std::mutex> lock(g_callback_mutex);
    std::vector<ClientCallbackEvent> result;
    result.swap(g_callback_events);
    return result;
}

}  // namespace nevergone::lua_runtime
