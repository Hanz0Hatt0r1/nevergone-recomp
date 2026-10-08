#include "choose_hero_background_timeline.h"

namespace nevergone::choose_hero_background_timeline {
namespace {

constexpr float storm_alpha_sample(double elapsed_seconds) {
    if (elapsed_seconds <= 0.0) return 0.0f;
    if (elapsed_seconds >= kStormFadeSeconds) return 1.0f;
    return static_cast<float>(elapsed_seconds / kStormFadeSeconds);
}

static_assert(kStormFadeSeconds == 6.0, "recovered PartThree fade must remain 6 seconds");
static_assert(storm_alpha_sample(-1.0) == 0.0f);
static_assert(storm_alpha_sample(0.0) == 0.0f);
static_assert(storm_alpha_sample(3.0) == 0.5f);
static_assert(storm_alpha_sample(6.0) == 1.0f);
static_assert(storm_alpha_sample(10.0) == 1.0f);

}  // namespace

float storm_alpha(double elapsed_seconds) {
    return storm_alpha_sample(elapsed_seconds);
}

}  // namespace nevergone::choose_hero_background_timeline
