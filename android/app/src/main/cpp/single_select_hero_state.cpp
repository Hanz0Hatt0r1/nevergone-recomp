#include "single_select_hero_state.h"

#include <mutex>
#include <sstream>

#include "character_name_state.h"

namespace nevergone::single_select_hero_state {
namespace {

std::mutex g_mutex;
bool g_active = false;
std::uint64_t g_generation = 0;
std::int64_t g_existing_career = 0;
std::int64_t g_selected_career = 0;
bool g_input_enabled = false;
bool g_transition_pending = false;
std::uint64_t g_selection_count = 0;
std::uint64_t g_confirm_count = 0;
std::uint64_t g_blocked_confirm_count = 0;
std::uint64_t g_character_name_open_count = 0;

std::int64_t normalize_existing_career(std::int64_t career) {
    return is_valid_career(career) ? career : 0;
}

Snapshot snapshot_locked() {
    Snapshot result;
    result.active = g_active;
    result.generation = g_generation;
    result.existing_career = g_existing_career;
    result.selected_career = g_selected_career;
    result.input_enabled = g_input_enabled;
    result.transition_pending = g_transition_pending;
    result.selection_count = g_selection_count;
    result.confirm_count = g_confirm_count;
    result.blocked_confirm_count = g_blocked_confirm_count;
    result.character_name_open_count = g_character_name_open_count;
    return result;
}

}  // namespace

void begin(std::int64_t existing_career) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_active = true;
    ++g_generation;
    g_existing_career = normalize_existing_career(existing_career);
    g_selected_career =
        g_existing_career == kCareerOne ? kCareerTwo : kCareerOne;

    // initUI() writes the interaction byte false before the initial Carousel.
    // A later visual transition executor restores it through OpenTheDoor(true).
    g_input_enabled = false;
    g_transition_pending = true;
    g_selection_count = 0;
    g_confirm_count = 0;
    g_blocked_confirm_count = 0;
    g_character_name_open_count = 0;
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_active = false;
    ++g_generation;
    g_existing_career = 0;
    g_selected_career = 0;
    g_input_enabled = false;
    g_transition_pending = false;
    g_selection_count = 0;
    g_confirm_count = 0;
    g_blocked_confirm_count = 0;
    g_character_name_open_count = 0;
}

bool is_valid_career(std::int64_t career) {
    return career >= kFirstCareer && career <= kLastCareer;
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

bool select_career(std::int64_t career) {
    if (!is_valid_career(career)) return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_active || !g_input_enabled) return false;
    if (g_selected_career == career) return true;

    g_selected_career = career;
    g_input_enabled = false;
    g_transition_pending = true;
    ++g_selection_count;
    return true;
}

bool confirm_online() {
    std::int64_t selected_career = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_active || !is_valid_career(g_selected_career)) return false;

        ++g_confirm_count;
        if (g_existing_career != 0 && g_existing_career == g_selected_career) {
            ++g_blocked_confirm_count;
            return false;
        }

        selected_career = g_selected_career;
        ++g_character_name_open_count;
    }

    // Avoid holding the selector mutex across the next reconstructed state
    // boundary. The shipped online branch passes this exact integer to
    // CharacterNameLayer::CretaUI(career).
    character_name_state::begin(selected_career);
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
    out << "single select hero: " << (state.active ? "active" : "inactive")
        << " generation=" << state.generation
        << " existing-career=" << state.existing_career
        << " selected-career=" << state.selected_career
        << " input=" << (state.input_enabled ? "enabled" : "locked")
        << " transition=" << (state.transition_pending ? "pending" : "idle")
        << "\n";
    out << "single select hero actions: selection=" << state.selection_count
        << " confirm=" << state.confirm_count
        << " blocked-confirm=" << state.blocked_confirm_count
        << " character-name-open=" << state.character_name_open_count
        << "\n";
    return out.str();
}

}  // namespace nevergone::single_select_hero_state
