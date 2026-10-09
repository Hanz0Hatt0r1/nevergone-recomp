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
// - changed career: close 2.0 + 0.3 + 0.39, then Carousel 2.4 = 5.09 s;
//   5.09 * 35 = 178.15, so the first fixed tick not earlier than the
//   callback point is tick + 179.
constexpr std::uint64_t kInitialUnlockTicks = 84;
constexpr std::uint64_t kCareerChangeUnlockTicks = 179;

struct Snapshot {
    Phase phase = Phase::kInactive;
    std::uint64_t selector_generation = 0;
    std::uint64_t begin_tick = 0;
    std::uint64_t unlock_tick = 0;
    std::uint64_t begin_count = 0;
    std::uint64_t completion_count = 0;
    std::uint64_t stale_generation_count = 0;
};

void reset();
void begin_initial(std::uint64_t tick, std::uint64_t selector_generation);
void begin_career_change(std::uint64_t tick, std::uint64_t selector_generation);

// Returns true exactly once when the recovered FunOpenTheDoor callback point
// is reached. A selector-generation mismatch invalidates the stale timeline.
bool advance(std::uint64_t tick, std::uint64_t selector_generation);

Snapshot snapshot();
const char* phase_name(Phase phase);
std::string status_report();

}  // namespace nevergone::single_select_hero_transition_timeline
