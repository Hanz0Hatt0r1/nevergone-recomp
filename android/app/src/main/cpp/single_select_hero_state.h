#pragma once

#include <cstdint>
#include <string>

namespace nevergone::single_select_hero_state {

constexpr std::int64_t kCareerOne = 1;
constexpr std::int64_t kCareerTwo = 2;

struct Snapshot {
    bool active = false;
    std::uint64_t generation = 0;
    std::int64_t existing_career = 0;
    std::int64_t selected_career = 0;
    std::uint64_t selection_count = 0;
    std::uint64_t confirm_count = 0;
    std::uint64_t blocked_confirm_count = 0;
    std::uint64_t character_name_open_count = 0;
};

// Mirrors SingleSelectHero::initUI() for the online create-role path. The
// shipped selector has exactly careers 1 and 2. If career 1 already exists it
// starts on career 2; otherwise it starts on career 1 (including when career 2
// already exists or when there is no existing career).
void begin(std::int64_t existing_career = 0);
void reset();

bool is_valid_career(std::int64_t career);

// Mirrors menuOpenGC(sender): the sender tag is the career itself and, once the
// original animation gate allows the click, it is copied unchanged into the
// selected-career fields. Selecting the already displayed career is a handled
// no-op.
bool select_career(std::int64_t career);

// Mirrors the online branch of menuConfirm(). Confirming the career already
// present in the existing role is blocked. Otherwise the selected career is
// passed unchanged to CharacterNameLayer::CretaUI(career), represented here by
// character_name_state::begin(career).
bool confirm_online();

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::single_select_hero_state
