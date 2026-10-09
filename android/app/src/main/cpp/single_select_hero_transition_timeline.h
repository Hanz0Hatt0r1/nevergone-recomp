#pragma once

#include <cstdint>
#include <string>

namespace nevergone::single_select_hero_transition_timeline {

enum class Phase : int {
    kInactive = 0,
    kInitialCarousel = 1,
    kCareerChange = 2,
};

// Recovered on the fixed 35 Hz game clock:
// - initial Carousel: 1.5 + 0.3 + 0.6 = 2.4 s = 84 ticks;
// - OpenTheDoor(false, oldCareer) close phase:
//     2.0 + 0.3 + 0.39 = 2.69 s;
// - changed-career Carousel(oldCareer):
//     |new-old| <= 2 -> 1.5 + 0.3 + 0.6 = 2.4 s;
//     |new-old| >  2 -> 3.0 + 0.3 + 0.6 = 3.9 s.
// Therefore the first fixed tick not earlier than FunOpenTheDoor is:
// - near: ceil((2.69 + 2.4) * 35) = 179 ticks;
// - far:  ceil((2.69 + 3.9) * 35) = 231 ticks.
constexpr std::uint64_t kInitialUnlockTicks = 84;
constexpr std::uint64_t kNearCareerChangeUnlockTicks = 179;
constexpr std::uint64_t kFarCareerChangeUnlockTicks = 231;

struct Snapshot {
    Phase phase = Phase::kInactive;
    std::uint64_t selector_generation = 0;
    std::uint64_t begin_tick = 0;
    std::uint64_t unlock_tick = 0;
    std::int64_t old_career = 0;
    std::int64_t new_career = 0;
    std::uint64_t begin_count = 0;
    std::uint64_t completion_count = 0;
    std::uint64_t stale_generation_count = 0;
};

bool valid_career(std::int64_t career);
std::uint64_t career_change_unlock_ticks(
    std::int64_t old_career,
    std::int64_t new_career);

void reset();
void begin_initial(
    std::uint64_t tick,
    std::uint64_t selector_generation,
    std::int64_t selected_career);
bool begin_career_change(
    std::uint64_t tick,
    std::uint64_t selector_generation,
    std::int64_t old_career,
    std::int64_t new_career);

// Returns true exactly once when the recovered FunOpenTheDoor callback point
// is reached. A selector-generation mismatch invalidates the stale timeline.
bool advance(std::uint64_t tick, std::uint64_t selector_generation);

Snapshot snapshot();
const char* phase_name(Phase phase);
std::string status_report();

}  // namespace nevergone::single_select_hero_transition_timeline
