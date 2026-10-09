#pragma once

#include <cstdint>
#include <string>

namespace nevergone::fresh_role_compat_state {

enum class Mode : int {
    kHidden = 0,
    kCareerSelection = 1,
    kCharacterName = 2,
};

struct Snapshot {
    Mode mode = Mode::kHidden;
    std::int64_t selected_career = 0;
    bool input_enabled = false;
    bool transition_pending = false;
    std::uint64_t career_generation = 0;
    std::uint64_t name_generation = 0;
    std::string role_name;
    bool randomize_pending = false;
};

// Temporary project-owned input bridge for the fresh-account path. It does not
// claim to reconstruct the shipped SingleSelectHero/CharacterNameLayer visual
// geometry. The missing door/carousel transition is deliberately collapsed to
// an immediate completion only when the compatibility UI asks to change class.
Snapshot snapshot();
bool select_career(std::int64_t career);
bool confirm_career();
bool set_role_name(std::string role_name);

const char* mode_name(Mode mode);
std::string status_report();

}  // namespace nevergone::fresh_role_compat_state
