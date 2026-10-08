#include "server_selection_state.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <mutex>
#include <sstream>

namespace nevergone::server_selection_state {
namespace {

std::mutex g_mutex;
login_callback_payload::ServerListPayload g_payload;
std::uint64_t g_payload_generation = 0;
int g_selected_index = -1;
bool g_touch_active = false;
int g_touch_pointer_id = -1;
float g_touch_begin_y = 0.0f;
std::uint64_t g_selection_changes = 0;
std::uint64_t g_rejected_drag_touches = 0;
std::uint64_t g_confirm_count = 0;
EnterRequest g_pending_request;

bool parse_server_id(const std::string& text, std::int64_t* output) {
    if (output == nullptr || text.empty()) return false;
    char* end = nullptr;
    errno = 0;
    const long long value = std::strtoll(text.c_str(), &end, 10);
    if (errno == ERANGE || end == nullptr || end == text.c_str() || *end != '\0') return false;
    *output = static_cast<std::int64_t>(value);
    return true;
}

void clear_touch_locked() {
    g_touch_active = false;
    g_touch_pointer_id = -1;
    g_touch_begin_y = 0.0f;
}

void clear_pending_locked() {
    g_pending_request = EnterRequest{};
}

bool select_index_locked(int index) {
    if (!g_payload.valid || index < 0 ||
            static_cast<std::size_t>(index) >= g_payload.servers.size()) {
        return false;
    }
    if (g_selected_index != index) {
        g_selected_index = index;
        ++g_selection_changes;
    }
    clear_pending_locked();
    return true;
}

Snapshot snapshot_locked() {
    Snapshot result;
    result.payload_valid = g_payload.valid;
    result.payload_generation = g_payload_generation;
    result.server_count = g_payload.servers.size();
    result.selected_index = g_selected_index;
    result.last_login_server = g_payload.last_login_server;
    result.touch_active = g_touch_active;
    result.touch_pointer_id = g_touch_pointer_id;
    result.touch_begin_y = g_touch_begin_y;
    result.selection_changes = g_selection_changes;
    result.rejected_drag_touches = g_rejected_drag_touches;
    result.confirm_count = g_confirm_count;
    result.enter_request_pending = g_pending_request.valid;

    if (g_selected_index >= 0 &&
            static_cast<std::size_t>(g_selected_index) < g_payload.servers.size()) {
        const auto& server = g_payload.servers[static_cast<std::size_t>(g_selected_index)];
        result.selected_server_id = server.id;
        result.selected_server_name = server.name;
        result.selected_server_ip = server.ip;
    }
    return result;
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_payload = login_callback_payload::ServerListPayload{};
    ++g_payload_generation;
    g_selected_index = -1;
    clear_touch_locked();
    g_selection_changes = 0;
    g_rejected_drag_touches = 0;
    g_confirm_count = 0;
    clear_pending_locked();
}

void sync_server_list(const login_callback_payload::ServerListPayload& payload) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_payload = payload;
    ++g_payload_generation;
    g_selected_index = -1;
    clear_touch_locked();
    clear_pending_locked();

    if (!g_payload.valid || g_payload.servers.empty()) return;

    std::int64_t last_login_id = 0;
    if (!parse_server_id(g_payload.last_login_server, &last_login_id)) return;
    for (std::size_t index = 0; index < g_payload.servers.size(); ++index) {
        if (g_payload.servers[index].id == last_login_id) {
            g_selected_index = static_cast<int>(index);
            return;
        }
    }
}

bool select_index(int index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return select_index_locked(index);
}

void touch_began(int pointer_id, float y) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_touch_active = true;
    g_touch_pointer_id = pointer_id;
    g_touch_begin_y = y;
}

bool touch_ended(int pointer_id, float y, int hit_index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_touch_active || pointer_id != g_touch_pointer_id) return false;

    const float movement = std::fabs(y - g_touch_begin_y);
    clear_touch_locked();
    if (movement > 10.0f) {
        ++g_rejected_drag_touches;
        return false;
    }
    return select_index_locked(hit_index);
}

void touch_cancelled(int pointer_id) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_touch_active && pointer_id == g_touch_pointer_id) clear_touch_locked();
}

EnterRequest confirm_selection() {
    std::lock_guard<std::mutex> lock(g_mutex);
    EnterRequest request;
    if (!g_payload.valid || g_selected_index < 0 ||
            static_cast<std::size_t>(g_selected_index) >= g_payload.servers.size()) {
        clear_pending_locked();
        return request;
    }

    const auto& server = g_payload.servers[static_cast<std::size_t>(g_selected_index)];
    if (server.ip.empty()) {
        clear_pending_locked();
        return request;
    }

    request.valid = true;
    request.server_id = server.id;
    request.server_name = server.name;
    request.ip = server.ip;
    g_pending_request = request;
    ++g_confirm_count;
    return request;
}

EnterRequest take_pending_enter_request() {
    std::lock_guard<std::mutex> lock(g_mutex);
    EnterRequest request = g_pending_request;
    clear_pending_locked();
    return request;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return snapshot_locked();
}

std::string status_report() {
    std::lock_guard<std::mutex> lock(g_mutex);
    const Snapshot state = snapshot_locked();
    std::ostringstream out;
    out << "server selection payload: "
        << (state.payload_valid ? "valid" : "inactive")
        << " generation=" << state.payload_generation
        << " servers=" << state.server_count << "\n";
    out << "server selection index: " << state.selected_index;
    if (state.selected_index >= 0) {
        out << " id=" << state.selected_server_id
            << " name=" << state.selected_server_name
            << " ip=" << state.selected_server_ip;
    }
    out << "\n";
    out << "server selection last-login: "
        << (state.last_login_server.empty() ? "none" : state.last_login_server) << "\n";
    out << "server selection touch: "
        << (state.touch_active ? "active" : "idle")
        << " rejected-drags=" << state.rejected_drag_touches << "\n";
    out << "server selection confirms: " << state.confirm_count
        << " pending-enter=" << (state.enter_request_pending ? "yes" : "no") << "\n";
    return out.str();
}

}  // namespace nevergone::server_selection_state
