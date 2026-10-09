#include "fresh_role_compat_state.h"

#include <sstream>
#include <utility>

#include "character_name_state.h"
#include "single_select_hero_state.h"

namespace nevergone::fresh_role_compat_state {

Snapshot snapshot() {
    const auto name = character_name_state::snapshot();
    const auto career = single_select_hero_state::snapshot();

    Snapshot result;
    result.career_generation = career.generation;
    result.name_generation = name.generation;
    result.selected_career = career.selected_career;
    result.input_enabled = career.input_enabled;
    result.transition_pending = career.transition_pending;
    result.role_name = name.role_name;
    result.randomize_pending = name.randomize_pending;

    // CharacterNameLayer sits above SingleSelectHero in the recovered flow, so
    // it takes precedence while both semantic states remain alive.
    if (name.active) {
        result.mode = Mode::kCharacterName;
    } else if (career.active) {
        result.mode = Mode::kCareerSelection;
    }
    return result;
}

bool select_career(std::int64_t career) {
    auto state = single_select_hero_state::snapshot();
    if (!state.active || !single_select_hero_state::is_valid_career(career)) {
        return false;
    }

    // The original menuOpenGC is gated by OpenTheDoor(). The visual transition
    // executor is not reconstructed yet, so this explicitly compatibility-only
    // path completes the pending transition before accepting a class tap.
    if (!state.input_enabled && state.transition_pending) {
        single_select_hero_state::complete_transition();
    }

    if (!single_select_hero_state::select_career(career)) {
        return false;
    }

    // A changed class starts another recovered transition. Collapse only this
    // project-owned compatibility path back to an interactive state; the native
    // compositor remains free to implement the real timing later.
    state = single_select_hero_state::snapshot();
    if (state.transition_pending) {
        single_select_hero_state::complete_transition();
    }
    return true;
}

bool confirm_career() {
    return single_select_hero_state::confirm_online();
}

bool set_role_name(std::string role_name) {
    return character_name_state::set_role_name(std::move(role_name));
}

const char* mode_name(Mode mode) {
    switch (mode) {
        case Mode::kHidden: return "hidden";
        case Mode::kCareerSelection: return "career-selection";
        case Mode::kCharacterName: return "character-name";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "fresh-role compatibility input: " << mode_name(state.mode)
        << " career=" << state.selected_career
        << " input=" << (state.input_enabled ? "enabled" : "locked")
        << " transition=" << (state.transition_pending ? "pending" : "idle")
        << " career-generation=" << state.career_generation
        << " name-generation=" << state.name_generation << "\n";
    if (state.mode == Mode::kCharacterName) {
        out << "fresh-role name bytes=" << state.role_name.size()
            << " randomize-pending=" << (state.randomize_pending ? "yes" : "no")
            << "\n";
    }
    return out.str();
}

}  // namespace nevergone::fresh_role_compat_state
