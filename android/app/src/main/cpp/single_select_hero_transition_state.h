#pragma once

#include <cstdint>
#include <string>

namespace nevergone::single_select_hero_transition_state {

constexpr double kDoorCloseSeconds = 2.0;
constexpr double kCarouselNearTravelSeconds = 1.5;
constexpr double kCarouselFarTravelSeconds = 3.0;
constexpr double kCarouselOvershootSeconds = 0.3;
constexpr double kCarouselReturnSeconds = 0.6;

struct Snapshot {
    bool active = false;
    std::uint64_t selector_generation = 0;
    std::uint64_t selector_selection_count = 0;
    std::int64_t from_career = 0;
    std::int64_t to_career = 0;
    bool includes_door_close = false;
    std::uint64_t start_tick = 0;
    double required_seconds = 0.0;
    double elapsed_seconds = 0.0;
    std::uint64_t completion_count = 0;
};

// Synchronizes the recovered OpenTheDoor/Carousel gate to the fixed runtime
// clock. Initial initUI Carousel has no preceding close-door delay. A changed
// menuOpenGC selection does: 2.0s close -> Carousel -> OpenTheDoor(true).
void sync(std::uint64_t tick);
void reset();

// Pure recovered duration helper, exposed for host smoke coverage.
double transition_seconds(
    std::int64_t from_career,
    std::int64_t to_career,
    bool includes_door_close);

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::single_select_hero_transition_state
