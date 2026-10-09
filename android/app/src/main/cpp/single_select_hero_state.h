#pragma once

#include <cstdint>
#include <string>

namespace nevergone::single_select_hero_state {

constexpr std::int64_t kCareerOne = 1;
constexpr std::int64_t kCareerTwo = 2;
constexpr std::int64_t kCareerThree = 3;
constexpr std::int64_t kCareerFour = 4;
constexpr std::int64_t kCareerFive = 5;
constexpr std::int64_t kFirstCareer = kCareerOne;
constexpr std::int64_t kLastCareer = kCareerFive;

struct Snapshot {
    bool active = false;
    std::uint64_t generation = 0;
    std::int64_t existing_career = 0;
    std::int64_t selected_career = 0;
    bool input_enabled = false;
    bool transition_pending = false;
    std::uint64_t selection_count = 0;
    std::uint64_t confirm_count = 0;
    std::uint64_t blocked_confirm_count = 0;
    std::uint64_t character_name_open_count = 0;
};

// Mirrors SingleSelectHero::initUI() for the online create-role path. The
// shipped UI creates five menuOpenGC items tagged with careers 1 through 5.
// Its initial candidate rule is narrower: if career 1 already exists it starts
// on career 2; otherwise it starts on career 1.
void begin(std::int64_t existing_career = 0);
void reset();

bool is_valid_career(std::int64_t career);
void set_input_enabled(bool enabled);
void complete_transition();
bool select_career(std::int64_t career);
bool confirm_online();

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::single_select_hero_state
