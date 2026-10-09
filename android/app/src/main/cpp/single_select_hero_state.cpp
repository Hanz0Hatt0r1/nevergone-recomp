#include "single_select_hero_state.h"

#include <mutex>
#include <sstream>

namespace nevergone::single_select_hero_state {
namespace {

std::mutex g_mutex;
bool g_active = false;
std::uint64_t g_generation = 0;
std::int32_t g_unavailable_career = 0;
std::int32_t g_current_career = 0;
std::int32_t g_selected_career = 0;
bool g_input_enabled = false;
bool g_transition_pending = false;
std::uint64_t g_selection_count = 0;
std::uint64_t g_confirm_count = 0;
std::uint64_t g_consume_count = 0;
std::uint64_t g_request_sequence = 0;
ConfirmRequest g_pending_confirm;

std::int32_t normalized_unavailable(std::int32_t career) {
    return valid_career(career) ? career : 0;
}

void clear_pending_locked() {
    g_pending_confirm = ConfirmRequest{};
}

Snapshot snapshot_locked() {
    Snapshot result;
    result.active = g_active;
    result.generation = g_generation;
    result.unavailable_career = g_unavailable_career;
    result.current_career = g_current_career;
    result.selected_career = g_selected_career;
    result.input_enabled = g_input_enabled;
    result.transition_pending = g_transition_pending;
    result.selection_count = g_selection_count;
    result.confirm_count = g_confirm_count;
    result.consume_count = g_consume_count;
    result.pending_confirm = g_pending_confirm;
    return result;
}

}  // namespace

bool valid_career(std::int32_t career) {
    return career == kCareerOne || career == kCareerTwo;
}

void begin(std::int32_t unavailable_career) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_active = true;
    ++g_generation;
    g_unavailable_career = normalized_unavailable(unavailable_career);
    g_selected_career = g_unavailable_career == kCareerOne ? kCareerTwo : kCareerOne;
    g_current_career = g_selected_career;

    // initUI() explicitly disables the interaction gate before entering the
    // initial Carousel(selectedCareer) presentation. The visual executor calls
    // complete_transition() when the recovered door/carousel transition ends.
    g_input_enabled = false;
    g_transition_pending = true;
    g_selection_count = 0;
    g_confirm_count = 0;
    g_consume_count = 0;
    g_request_sequence = 0;
    clear_pending_locked();
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_active = false;
    ++g_generation;
    g_unavailable_career = 0;
    g_current_career = 0;
    g_selected_career = 0;
    g_input_enabled = false;
    g_transition_pending = false;
    g_selection_count = 0;
    g_confirm_count = 0;
    g_consume_count = 0;
    g_request_sequence = 0;
    clear_pending_locked();
}

void set_input_enabled(bool enabled) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_active) return;
    g_input_enabled = enabled;
    if (enabled) g_transition_pending = false;
}

void complete_transition() {
    set_input_enabled(true);
}

bool select_career(std::int32_t career) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_active || !g_input_enabled || !valid_career(career) ||
            career == g_unavailable_career || career == g_selected_career) {
        return false;
    }

    g_current_career = career;
    g_selected_career = career;
    g_input_enabled = false;
    g_transition_pending = true;
    ++g_selection_count;
    clear_pending_locked();
    return true;
}

bool confirm() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_active || !valid_career(g_current_career) ||
            g_current_career != g_selected_career ||
            g_current_career == g_unavailable_career) {
        clear_pending_locked();
        return false;
    }

    ConfirmRequest request;
    request.valid = true;
    request.generation = g_generation;
    request.sequence = ++g_request_sequence;
    request.career = g_current_career;
    g_pending_confirm = request;
    ++g_confirm_count;
    return true;
}

bool peek_confirm(ConfirmRequest* output) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_pending_confirm.valid) return false;
    if (output != nullptr) *output = g_pending_confirm;
    return true;
}

bool take_confirm(ConfirmRequest* output) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_pending_confirm.valid) return false;
    if (output != nullptr) *output = g_pending_confirm;
    clear_pending_locked();
    ++g_consume_count;
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
    out << "SingleSelectHero career state: " << (state.active ? "active" : "inactive")
        << " generation=" << state.generation
        << " unavailable=" << state.unavailable_career
        << " current=" << state.current_career
        << " selected=" << state.selected_career << "\n";
    out << "SingleSelectHero input=" << (state.input_enabled ? "enabled" : "locked")
        << " transition=" << (state.transition_pending ? "pending" : "idle")
        << " selections=" << state.selection_count
        << " confirms=" << state.confirm_count
        << " consumes=" << state.consume_count
        << " confirm-pending=" << (state.pending_confirm.valid ? "yes" : "no")
        << "\n";
    return out.str();
}

}  // namespace nevergone::single_select_hero_state
