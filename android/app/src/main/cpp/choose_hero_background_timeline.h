#pragma once

namespace nevergone::choose_hero_background_timeline {

constexpr double kStormFadeSeconds = 6.0;

// Recovered ChooseHeroBackground::PartThree behavior: bejingwuyun.png starts
// at opacity 0 and runs CCFadeIn(6.0f).
float storm_alpha(double elapsed_seconds);

}  // namespace nevergone::choose_hero_background_timeline
