#include "role_selection_state.h"

#include <mutex>
#include <sstream>

namespace nevergone::role_selection_state {
namespace {

std::mutex g_mutex;
login_callback_payload::RoleListPayload g_payload;
std::uint64_t g_payload_generation = 0;
int g_selected_index = -1;
std::uint64_t g_selection_changes = 0;
std::uint64_t g_confirm_count = 0;
std::uint64_t g_create_request_count = 0;
EnterRoleRequest g_pending_enter;
CreateRoleRequest g_pending_create;

void clear_pending_locked() {
    g_pending_enter = EnterRoleRequest{};
    g_pending_create = CreateRoleRequest{};
}

bool select_career_locked(std::int64_t career) {
    if (!g_payload.valid) return false;
    for (std::size_t index = 0; index < g_payload.roles.size(); ++index) {
        if (g_payload.roles[index].career == career) {
            const int resolved = static_cast<int>(index);
            if (g_selected_index != resolved) {
                g_selected_index = resolved;
                ++g_selection_changes;
            }
            clear_pending_locked();
            return true;
        }
    }
    return false;
}

Snapshot snapshot_locked() {
    Snapshot result;
    result.payload_valid = g_payload.valid;
    result.payload_generation = g_payload_generation;
    result.role_count = g_payload.roles.size();
    result.selected_index = g_selected_index;
    result.selection_changes = g_selection_changes;
    result.confirm_count = g_confirm_count;
    result.create_request_count = g_create_request_count;
    result.enter_request_pending = g_pending_enter.valid;
    result.create_request_pending = g_pending_create.valid;
    if (g_selected_index >= 0 &&
            static_cast<std::size_t>(g_selected_index) < g_payload.roles.size()) {
        const auto& role = g_payload.roles[static_cast<std::size_t>(g_selected_index)];
        result.selected_career = role.career;
        result.selected_character_id = role.character_id;
        result.selected_character_name = role.character_name;
    }
    return result;
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_payload = login_callback_payload::RoleListPayload{};
    ++g_payload_generation;
    g_selected_index = -1;
    g_selection_changes = 0;
    g_confirm_count = 0;
    g_create_request_count = 0;
    clear_pending_locked();
}

void sync_role_list(const login_callback_payload::RoleListPayload& payload) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_payload = payload;
    ++g_payload_generation;
    g_selected_index = -1;
    clear_pending_locked();
}

bool select_career(std::int64_t career) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return select_career_locked(career);
}

bool select_index(int index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_payload.valid || index < 0 ||
            static_cast<std::size_t>(index) >= g_payload.roles.size()) {
        return false;
    }
    // Match ChooseHero: selecting an item stores its career tag, then the
    // eventual start path resolves the first hero record with that career.
    return select_career_locked(g_payload.roles[static_cast<std::size_t>(index)].career);
}

EnterRoleRequest confirm_selection() {
    std::lock_guard<std::mutex> lock(g_mutex);
    EnterRoleRequest request;
    if (!g_payload.valid || g_selected_index < 0 ||
            static_cast<std::size_t>(g_selected_index) >= g_payload.roles.size()) {
        g_pending_enter = EnterRoleRequest{};
        return request;
    }

    const auto& role = g_payload.roles[static_cast<std::size_t>(g_selected_index)];
    request.valid = true;
    request.character_id = role.character_id;
    request.career = role.career;
    request.character_name = role.character_name;
    g_pending_enter = request;
    g_pending_create = CreateRoleRequest{};
    ++g_confirm_count;
    return request;
}

EnterRoleRequest peek_pending_enter_request() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_pending_enter;
}

EnterRoleRequest take_pending_enter_request() {
    std::lock_guard<std::mutex> lock(g_mutex);
    EnterRoleRequest result = g_pending_enter;
    g_pending_enter = EnterRoleRequest{};
    return result;
}

bool request_create_role(const std::string& character_name, std::int64_t career) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (character_name.empty()) return false;
    g_pending_create.valid = true;
    g_pending_create.character_name = character_name;
    g_pending_create.career = career;
    g_pending_enter = EnterRoleRequest{};
    ++g_create_request_count;
    return true;
}

CreateRoleRequest peek_pending_create_request() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_pending_create;
}

CreateRoleRequest take_pending_create_request() {
    std::lock_guard<std::mutex> lock(g_mutex);
    CreateRoleRequest result = g_pending_create;
    g_pending_create = CreateRoleRequest{};
    return result;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return snapshot_locked();
}

std::string status_report() {
    std::lock_guard<std::mutex> lock(g_mutex);
    const Snapshot state = snapshot_locked();
    std::ostringstream out;
    out << "role selection payload: " << (state.payload_valid ? "valid" : "inactive")
        << " generation=" << state.payload_generation
        << " roles=" << state.role_count << "\n";
    out << "role selection index: " << state.selected_index;
    if (state.selected_index >= 0) {
        out << " career=" << state.selected_career
            << " character-id=" << state.selected_character_id
            << " name=" << state.selected_character_name;
    }
    out << "\n";
    out << "role selection changes: " << state.selection_changes
        << " confirms=" << state.confirm_count
        << " enter-pending=" << (state.enter_request_pending ? "yes" : "no")
        << " create-pending=" << (state.create_request_pending ? "yes" : "no")
        << "\n";
    return out.str();
}

}  // namespace nevergone::role_selection_state
