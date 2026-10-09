#include "client_callback_bridge.h"

#include <mutex>
#include <sstream>
#include <utility>

#include "character_name_state.h"
#include "created_role_transition.h"
#include "initial_ui_transition.h"
#include "role_selection_state.h"
#include "server_selection_state.h"

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lua.h>
}
#endif

namespace nevergone::lua_runtime {
namespace {

std::mutex g_callback_mutex;
std::vector<ClientCallbackEvent> g_callback_events;
ClientUiSnapshot g_client_ui_state;

std::string join_arguments(const std::vector<std::string>& arguments) {
    std::ostringstream out;
    for (size_t index = 0; index < arguments.size(); ++index) {
        if (index != 0) out << " | ";
        out << arguments[index];
    }
    return out.str();
}

void apply_event_to_ui_state(const ClientCallbackEvent& event) {
    ++g_client_ui_state.event_count;
    g_client_ui_state.last_event = event.name;
    const std::string payload = join_arguments(event.arguments);

    if (event.name == "cpp_OnGetServerList") {
        g_client_ui_state.server_list = payload;
        login_callback_payload::parse_server_list_callback(
            event.arguments,
            &g_client_ui_state.server_list_model);
        nevergone::server_selection_state::sync_server_list(
            g_client_ui_state.server_list_model);
    } else if (event.name == "cpp_OnGetRoleList") {
        g_client_ui_state.role_list = payload;
        login_callback_payload::parse_role_list_callback(
            event.arguments,
            &g_client_ui_state.role_list_model);
        nevergone::role_selection_state::sync_role_list(
            g_client_ui_state.role_list_model);
    } else if (event.name == "cpp_OnCreateTheRole") {
        g_client_ui_state.created_role = payload;
    } else if (event.name == "cpp_OnGameAnnoucement") {
        g_client_ui_state.announcement = payload;
    } else if (event.name == "cpp_OnEnterGame") {
        g_client_ui_state.enter_game = payload;
    } else if (event.name == "cpp_OnReceivedChatMessages") {
        g_client_ui_state.chat_messages = payload;
    } else if (event.name == "cpp_OnUpdateData") {
        g_client_ui_state.update_data = payload;
    } else if (event.name == "cpp_connect_pve") {
        g_client_ui_state.pve_connect = payload;
    }

    nevergone::initial_ui_transition::on_management_callback(event.name);
}

void filter_login_payloads_for_management_route(ClientUiSnapshot* state) {
    if (state == nullptr) return;
    const auto route = nevergone::initial_ui_transition::snapshot().management_route;
    using nevergone::initial_ui_transition::ManagementRoute;

    if (route != ManagementRoute::kAnnouncement) state->announcement.clear();
    if (route != ManagementRoute::kServerSelection) {
        state->server_list.clear();
        state->server_list_model = login_callback_payload::ServerListPayload{};
    }
    if (route != ManagementRoute::kRoleSelection) {
        state->role_list.clear();
        state->role_list_model = login_callback_payload::RoleListPayload{};
    }
    if (route != ManagementRoute::kRoleCreated) state->created_role.clear();
    if (route != ManagementRoute::kEnteringGame) state->enter_game.clear();
}

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

    login_callback_payload::RoleEntry created_role;
    bool has_created_role = false;
    if (event.name == "cpp_OnCreateTheRole") {
        // The shipped Lua callback passes cjson.encode(tBaseInfo). Reuse the
        // existing bounded role parser: a valid creation callback contains one
        // unique CharacterID record, even if the object has nested metadata.
        login_callback_payload::RoleListPayload parsed;
        if (login_callback_payload::parse_role_list_callback(event.arguments, &parsed) &&
                parsed.valid && parsed.roles.size() == 1 &&
                parsed.roles[0].character_id != 0) {
            created_role = parsed.roles[0];
            has_created_role = true;
        }
    }

    {
        std::lock_guard<std::mutex> lock(g_callback_mutex);
        apply_event_to_ui_state(event);
        g_callback_events.push_back(event);
    }

    // CreateTheRoleSuccessful removes CharacterName and starts the returned
    // role immediately. Stage that follow-up after releasing the callback
    // mutex, but defer the Lua dispatch itself until the next runtime pump so
    // a callback cannot recursively lock the persistent Lua session.
    if (has_created_role) {
        (void)nevergone::character_name_state::complete_creation();
        (void)nevergone::created_role_transition::stage(created_role);
    }
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
        g_client_ui_state = ClientUiSnapshot{};
    }
    nevergone::server_selection_state::reset();
    nevergone::role_selection_state::reset();
    nevergone::created_role_transition::reset();
    register_callback(state, "cpp_OnGetServerList");
    register_callback(state, "cpp_OnGetRoleList");
    register_callback(state, "cpp_OnCreateTheRole");
    register_callback(state, "cpp_OnGameAnnoucement");
    register_callback(state, "cpp_OnEnterGame");
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

ClientUiSnapshot snapshot_client_ui_state() {
    std::lock_guard<std::mutex> lock(g_callback_mutex);
    ClientUiSnapshot projected = g_client_ui_state;
    filter_login_payloads_for_management_route(&projected);
    return projected;
}

void reset_client_ui_state() {
    {
        std::lock_guard<std::mutex> lock(g_callback_mutex);
        g_client_ui_state = ClientUiSnapshot{};
    }
    nevergone::server_selection_state::reset();
    nevergone::role_selection_state::reset();
    nevergone::created_role_transition::reset();
}

std::string client_ui_state_report() {
    const ClientUiSnapshot snapshot = snapshot_client_ui_state();
    const auto transition = nevergone::initial_ui_transition::snapshot();
    std::ostringstream out;
    out << "events captured: " << snapshot.event_count << "\n";
    out << "last callback: " << (snapshot.last_event.empty() ? "none" : snapshot.last_event) << "\n";
    out << "management UI route: "
        << nevergone::initial_ui_transition::management_route_name(transition.management_route) << "\n";
    if (!snapshot.server_list.empty()) out << "server list: " << snapshot.server_list << "\n";
    if (snapshot.server_list_model.valid) {
        out << "structured servers: " << snapshot.server_list_model.servers.size() << "\n";
        out << "last login server: "
            << (snapshot.server_list_model.last_login_server.empty()
                    ? "none"
                    : snapshot.server_list_model.last_login_server)
            << "\n";
    }
    if (!snapshot.role_list.empty()) out << "role list: " << snapshot.role_list << "\n";
    if (snapshot.role_list_model.valid) {
        out << "structured roles: " << snapshot.role_list_model.roles.size() << "\n";
    }
    if (!snapshot.created_role.empty()) out << "created role: " << snapshot.created_role << "\n";
    if (!snapshot.announcement.empty()) out << "announcement: " << snapshot.announcement << "\n";
    if (!snapshot.enter_game.empty()) out << "enter game: " << snapshot.enter_game << "\n";
    if (!snapshot.chat_messages.empty()) out << "chat: " << snapshot.chat_messages << "\n";
    if (!snapshot.update_data.empty()) out << "data update: " << snapshot.update_data << "\n";
    if (!snapshot.pve_connect.empty()) out << "PVE connect: " << snapshot.pve_connect << "\n";
    if (transition.management_route == nevergone::initial_ui_transition::ManagementRoute::kServerSelection) {
        out << nevergone::server_selection_state::status_report();
    }
    if (transition.management_route == nevergone::initial_ui_transition::ManagementRoute::kRoleSelection ||
            transition.management_route == nevergone::initial_ui_transition::ManagementRoute::kRoleCreated) {
        out << nevergone::role_selection_state::status_report();
    }
    if (transition.management_route == nevergone::initial_ui_transition::ManagementRoute::kRoleCreated) {
        out << nevergone::created_role_transition::status_report();
    }
    return out.str();
}

}  // namespace nevergone::lua_runtime
