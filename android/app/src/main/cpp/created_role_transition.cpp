#include "created_role_transition.h"

#include <mutex>
#include <sstream>
#include <utility>

namespace nevergone::created_role_transition {
namespace {

std::mutex g_mutex;
Snapshot g_state;

bool same_role(
        const login_callback_payload::RoleEntry& left,
        const login_callback_payload::RoleEntry& right) {
    return left.character_id == right.character_id &&
        left.career == right.career &&
        left.character_name == right.character_name;
}

void copy_error(std::string* output, const std::string& value) {
    if (output != nullptr) *output = value;
}

}  // namespace

bool stage(const login_callback_payload::RoleEntry& role) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (role.character_id == 0) {
        g_state.last_error = "created role has no CharacterID";
        return false;
    }
    g_state.pending = true;
    g_state.dispatch_due = true;
    g_state.role = role;
    ++g_state.stage_count;
    g_state.last_error.clear();
    return true;
}

bool request_retry() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_state.pending || g_state.dispatch_due) return false;
    g_state.dispatch_due = true;
    return true;
}

Outcome pump_with_dispatch(DispatchFn dispatch, std::string* error) {
    login_callback_payload::RoleEntry role;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_state.pending || !g_state.dispatch_due) {
            if (error != nullptr) error->clear();
            return Outcome::kIdle;
        }
        role = g_state.role;
        g_state.dispatch_due = false;
    }

    std::string dispatch_error;
    if (dispatch == nullptr || !dispatch(role, &dispatch_error)) {
        std::lock_guard<std::mutex> lock(g_mutex);
        ++g_state.dispatch_failure_count;
        g_state.last_error = dispatch_error.empty()
            ? "created-role enter dispatch failed"
            : dispatch_error;
        copy_error(error, g_state.last_error);
        return Outcome::kDispatchFailed;
    }

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_state.pending || !same_role(g_state.role, role)) {
            ++g_state.dispatch_failure_count;
            g_state.last_error = "created role changed during enter dispatch";
            copy_error(error, g_state.last_error);
            return Outcome::kDispatchFailed;
        }
        g_state.pending = false;
        g_state.dispatch_due = false;
        ++g_state.dispatch_count;
        g_state.last_error.clear();
    }
    if (error != nullptr) error->clear();
    return Outcome::kDispatched;
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = Snapshot{};
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

const char* outcome_name(Outcome outcome) {
    switch (outcome) {
        case Outcome::kIdle: return "idle";
        case Outcome::kDispatchFailed: return "dispatch-failed";
        case Outcome::kDispatched: return "dispatched";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "created-role transition: " << (state.pending ? "pending" : "idle")
        << " due=" << (state.dispatch_due ? "yes" : "no")
        << " staged=" << state.stage_count
        << " dispatched=" << state.dispatch_count
        << " failures=" << state.dispatch_failure_count;
    if (state.pending) {
        out << " character-id=" << state.role.character_id
            << " career=" << state.role.career
            << " name=" << state.role.character_name;
    }
    out << "\n";
    if (!state.last_error.empty()) {
        out << "created-role transition last error: " << state.last_error << "\n";
    }
    return out.str();
}

}  // namespace nevergone::created_role_transition
