#include "choose_hero_background_timeline.h"

#include <algorithm>

namespace nevergone::choose_hero_background_timeline {

float storm_alpha(double elapsed_seconds) {
    if (elapsed_seconds <= 0.0) return 0.0f;
    if (elapsed_seconds >= kStormFadeSeconds) return 1.0f;
    return static_cast<float>(elapsed_seconds / kStormFadeSeconds);
}

}  // namespace nevergone::choose_hero_background_timeline
