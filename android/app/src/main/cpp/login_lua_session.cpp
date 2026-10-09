#include "login_lua_session.h"

#include <filesystem>
#include <mutex>
#include <sstream>

#include "client_callback_bridge.h"
#include "login_lua_dispatch.h"
#include "lua_startup_bindings.h"
#include "native_binding_registry.h"
#include "role_selection_state.h"
#include "server_selection_state.h"
#include "startup_contract.h"

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}
#endif

namespace nevergone::login_lua_session {
namespace {

std::mutex g_mutex;
Phase g_phase = Phase::kStopped;
std::uint64_t g_generation = 0;
std::uint64_t g_start_count = 0;
std::uint64_t g_dispatch_count = 0;
std::uint64_t g_dispatch_failure_count = 0;
std::string g_last_error;
std::string g_startup_report = "persistent login Lua: not started\n";

#if defined(NEVERGONE_HAS_LUA)
lua_State* g_state = nullptr;
#endif

void close_state_locked() {
#if defined(NEVERGONE_HAS_LUA)
    if (g_state != nullptr) {
        lua_close(g_state);
        g_state = nullptr;
    }
#endif
}

void append_startup_observations(std::ostringstream* out) {
    if (out == nullptr) return;
    const auto modules = startup::take_requested_modules();
    const auto ui_events = startup::take_ui_events();
    const auto callback_events = lua_runtime::take_client_callback_events();
    const auto missing_globals = lua_runtime::take_missing_globals();

    *out << "startup modules requested: " << modules.size() << "\n";
    for (const auto& module : modules) *out << "  - " << module << "\n";
    *out << "startup UI events: " << ui_events.size() << "\n";
    *out << "login callback events: " << callback_events.size() << "\n";
    for (const auto& event : callback_events) {
        *out << "  - " << event.name;
        if (!event.arguments.empty()) {
            *out << "(";
            for (std::size_t index = 0; index < event.arguments.size(); ++index) {
                if (index != 0) *out << ", ";
                *out << event.arguments[index];
            }
            *out << ")";
        }
        *out << "\n";
    }
    *out << "missing globals observed: " << missing_globals.size() << "\n";
    for (const auto& item : missing_globals) {
        *out << "  - " << item.name << " (" << item.hits << ")\n";
    }
}

Snapshot snapshot_locked() {
    Snapshot result;
    result.phase = g_phase;
    result.generation = g_generation;
    result.start_count = g_start_count;
    result.dispatch_count = g_dispatch_count;
    result.dispatch_failure_count = g_dispatch_failure_count;
    result.last_error = g_last_error;
    return result;
}

bool require_ready_locked() {
    if (g_phase == Phase::kReady) return true;
    g_last_error = "persistent login Lua is not ready";
    ++g_dispatch_failure_count;
    return false;
}

}  // namespace

bool ensure_started() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_phase == Phase::kReady) return true;

    close_state_locked();
    g_phase = Phase::kStarting;
    g_last_error.clear();
    ++g_generation;
    ++g_start_count;

#if defined(NEVERGONE_HAS_LUA)
    const auto& runtime = startup::config();
    const std::filesystem::path start_lua =
        std::filesystem::path(runtime.files_dir) / "assets" / "Script" / "Game" / "StartLua.lua";
    if (runtime.files_dir.empty() || !std::filesystem::is_regular_file(start_lua)) {
        g_phase = Phase::kFailed;
        g_last_error = "Game/StartLua.lua not imported";
        g_startup_report = "persistent login Lua: failed (" + g_last_error + ")\n";
        return false;
    }

    g_state = luaL_newstate();
    if (g_state == nullptr) {
        g_phase = Phase::kFailed;
        g_last_error = "luaL_newstate failed";
        g_startup_report = "persistent login Lua: failed (" + g_last_error + ")\n";
        return false;
    }

    luaL_openlibs(g_state);
    lua_runtime::register_native_bindings(g_state);
    lua_runtime::install_missing_global_probe(g_state);

    std::string error;
    const bool ok = lua_runtime::execute_module(g_state, "Game.StartLua", &error);
    std::ostringstream report;
    report << "persistent login Lua: " << (ok ? "ready" : "failed") << "\n";
    append_startup_observations(&report);
    if (!ok) {
        g_phase = Phase::kFailed;
        g_last_error = error.empty() ? "Game.StartLua failed" : error;
        report << "startup traceback:\n" << g_last_error << "\n";
        close_state_locked();
        g_startup_report = report.str();
        return false;
    }

    g_phase = Phase::kReady;
    g_startup_report = report.str();
    return true;
#else
    g_phase = Phase::kFailed;
    g_last_error = "Lua runtime unavailable";
    g_startup_report = "persistent login Lua: skipped (Lua unavailable)\n";
    return false;
#endif
}

void shutdown() {
    std::lock_guard<std::mutex> lock(g_mutex);
    close_state_locked();
    g_phase = Phase::kStopped;
    g_last_error.clear();
    g_startup_report = "persistent login Lua: not started\n";
    ++g_generation;
}

bool dispatch_pending_server_request() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!require_ready_locked()) return false;

    const auto request = server_selection_state::peek_pending_enter_request();
    if (!request.valid) {
        g_last_error = "no pending server enter request";
        return false;
    }

#if defined(NEVERGONE_HAS_LUA)
    std::string error;
    if (!login_lua_dispatch::call_enter_game_logic_server(
            g_state, request.ip, request.server_id, &error)) {
        g_last_error = error;
        ++g_dispatch_failure_count;
        return false;
    }

    const auto consumed = server_selection_state::take_pending_enter_request();
    if (!consumed.valid || consumed.server_id != request.server_id || consumed.ip != request.ip) {
        g_last_error = "pending server request changed during dispatch";
        ++g_dispatch_failure_count;
        return false;
    }

    ++g_dispatch_count;
    g_last_error.clear();
    return true;
#else
    g_last_error = "Lua runtime unavailable";
    ++g_dispatch_failure_count;
    return false;
#endif
}

bool dispatch_pending_role_enter_request() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!require_ready_locked()) return false;

    const auto request = role_selection_state::peek_pending_enter_request();
    if (!request.valid) {
        g_last_error = "no pending role enter request";
        return false;
    }

#if defined(NEVERGONE_HAS_LUA)
    std::string error;
    if (!login_lua_dispatch::call_enter_game_with_cid(
            g_state, request.character_id, &error)) {
        g_last_error = error;
        ++g_dispatch_failure_count;
        return false;
    }

    const auto consumed = role_selection_state::take_pending_enter_request();
    if (!consumed.valid || consumed.character_id != request.character_id ||
            consumed.career != request.career || consumed.character_name != request.character_name) {
        g_last_error = "pending role enter request changed during dispatch";
        ++g_dispatch_failure_count;
        return false;
    }

    ++g_dispatch_count;
    g_last_error.clear();
    return true;
#else
    g_last_error = "Lua runtime unavailable";
    ++g_dispatch_failure_count;
    return false;
#endif
}

bool dispatch_pending_role_create_request() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!require_ready_locked()) return false;

    const auto request = role_selection_state::peek_pending_create_request();
    if (!request.valid) {
        g_last_error = "no pending role create request";
        return false;
    }

#if defined(NEVERGONE_HAS_LUA)
    std::string error;
    if (!login_lua_dispatch::call_create_character(
            g_state, request.character_name, request.career, &error)) {
        g_last_error = error;
        ++g_dispatch_failure_count;
        return false;
    }

    const auto consumed = role_selection_state::take_pending_create_request();
    if (!consumed.valid || consumed.career != request.career ||
            consumed.character_name != request.character_name) {
        g_last_error = "pending role create request changed during dispatch";
        ++g_dispatch_failure_count;
        return false;
    }

    ++g_dispatch_count;
    g_last_error.clear();
    return true;
#else
    g_last_error = "Lua runtime unavailable";
    ++g_dispatch_failure_count;
    return false;
#endif
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return snapshot_locked();
}

const char* phase_name(Phase phase) {
    switch (phase) {
        case Phase::kStopped: return "stopped";
        case Phase::kStarting: return "starting";
        case Phase::kReady: return "ready";
        case Phase::kFailed: return "failed";
    }
    return "unknown";
}

std::string startup_report() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_startup_report;
}

std::string status_report() {
    std::lock_guard<std::mutex> lock(g_mutex);
    const Snapshot state = snapshot_locked();
    std::ostringstream out;
    out << "persistent login Lua: " << phase_name(state.phase)
        << " generation=" << state.generation
        << " starts=" << state.start_count << "\n";
    out << "login Lua dispatches: " << state.dispatch_count
        << " failures=" << state.dispatch_failure_count << "\n";
    if (!state.last_error.empty()) out << "login Lua last error: " << state.last_error << "\n";
    return out.str();
}

}  // namespace nevergone::login_lua_session