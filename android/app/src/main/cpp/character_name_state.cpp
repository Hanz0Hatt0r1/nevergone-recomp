#include "character_name_state.h"

#include <mutex>
#include <sstream>
#include <utility>

#include "role_selection_state.h"

namespace nevergone::character_name_state {
namespace {

std::mutex g_mutex;
bool g_active = false;
std::uint64_t g_generation = 0;
std::int64_t g_career = 0;
std::string g_role_name;
std::uint64_t g_action_count = 0;
std::uint64_t g_submit_count = 0;
std::uint64_t g_close_count = 0;
std::uint64_t g_completion_count = 0;
std::uint64_t g_randomize_count = 0;
bool g_randomize_pending = false;
role_creation_validation::Snapshot g_last_validation;

Snapshot snapshot_locked() {
    Snapshot result;
    result.active = g_active;
    result.generation = g_generation;
    result.career = g_career;
    result.role_name = g_role_name;
    result.action_count = g_action_count;
    result.submit_count = g_submit_count;
    result.close_count = g_close_count;
    result.completion_count = g_completion_count;
    result.randomize_count = g_randomize_count;
    result.randomize_pending = g_randomize_pending;
    result.last_validation = g_last_validation;
    return result;
}

}  // namespace

void begin(std::int64_t career) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_active = true;
    ++g_generation;
    g_career = career;
    g_role_name.clear();
    g_action_count = 0;
    g_submit_count = 0;
    g_close_count = 0;
    g_completion_count = 0;
    g_randomize_count = 1;
    g_randomize_pending = true;
    g_last_validation = role_creation_validation::Snapshot{};
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_active = false;
    ++g_generation;
    g_career = 0;
    g_role_name.clear();
    g_action_count = 0;
    g_submit_count = 0;
    g_close_count = 0;
    g_completion_count = 0;
    g_randomize_count = 0;
    g_randomize_pending = false;
    g_last_validation = role_creation_validation::Snapshot{};
}

bool complete_creation() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_active) return false;
    g_active = false;
    g_randomize_pending = false;
    ++g_completion_count;
    return true;
}

bool set_role_name(std::string value) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_active) return false;
    g_role_name = std::move(value);
    return true;
}

bool action_from_tag(int tag, Action* output) {
    Action action = Action::kNone;
    switch (tag) {
        case 1: action = Action::kSubmit; break;
        case 2: action = Action::kClose; break;
        case 3: action = Action::kRandomize; break;
        default: return false;
    }
    if (output != nullptr) *output = action;
    return true;
}

const char* action_name(Action action) {
    switch (action) {
        case Action::kNone: return "none";
        case Action::kSubmit: return "submit";
        case Action::kClose: return "close";
        case Action::kRandomize: return "randomize";
    }
    return "unknown";
}

bool dispatch_tag(int tag) {
    Action action = Action::kNone;
    if (!action_from_tag(tag, &action)) return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_active) return false;
    ++g_action_count;

    switch (action) {
        case Action::kSubmit: {
            ++g_submit_count;
            g_last_validation = role_creation_validation::validate_role_name_with_max_bytes(
                    g_role_name, kMaxCharacterNameBytes);
            if (g_last_validation.result != role_creation_validation::Result::kValid) {
                return true;
            }
            role_selection_state::request_create_role(g_role_name, g_career);
            return true;
        }
        case Action::kClose:
            ++g_close_count;
            g_active = false;
            g_randomize_pending = false;
            return true;
        case Action::kRandomize:
            ++g_randomize_count;
            g_randomize_pending = true;
            return true;
        case Action::kNone:
            break;
    }
    return false;
}

bool randomize_pending() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_randomize_pending;
}

bool take_randomize_request(std::int64_t* career) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_active || !g_randomize_pending) return false;
    if (career != nullptr) *career = g_career;
    g_randomize_pending = false;
    return true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return snapshot_locked();
}

std::string status_report() {
    std::lock_guard<std::mutex> lock(g_mutex);
    const Snapshot state = snapshot_locked();
    std::ostringstream out;
    out << "character name layer: " << (state.active ? "active" : "inactive")
        << " generation=" << state.generation
        << " career=" << state.career << "\n";
    out << "character name bytes=" << state.role_name.size()
        << " validation=" << role_creation_validation::result_name(state.last_validation.result)
        << " actions=" << state.action_count
        << " submit=" << state.submit_count
        << " close=" << state.close_count
        << " completion=" << state.completion_count
        << " randomize=" << state.randomize_count
        << " randomize-pending=" << (state.randomize_pending ? "yes" : "no")
        << "\n";
    return out.str();
}

}  // namespace nevergone::character_name_state
