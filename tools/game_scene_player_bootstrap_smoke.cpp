#include <cassert>
#include <cmath>
#include <string>

#include "game_scene_player_bootstrap.h"

namespace bootstrap = nevergone::game_scene_player_bootstrap;

int main() {
    assert(!bootstrap::valid_offline_career(0u));
    assert(bootstrap::valid_offline_career(1u));
    assert(bootstrap::valid_offline_career(5u));
    assert(!bootstrap::valid_offline_career(6u));
    assert(std::string(bootstrap::player_name()) == "SELF");
    assert(bootstrap::kConstructionResourceSelector == 1u);
    assert(bootstrap::kConstructionComparedResourceField == 0u);
    assert(bootstrap::kInitialActionState == 0x1bu);

    const auto point = bootstrap::initial_manager_point(1136.0f, 640.0f);
    assert(point.has_value());
    assert(point->x == 568.0f);
    assert(std::fabs(point->y - 448.0f) < 0.001f);
    assert(!bootstrap::initial_manager_point(0.0f, 640.0f).has_value());
    assert(!bootstrap::initial_manager_point(1136.0f, -1.0f).has_value());
    assert(!bootstrap::initial_manager_point(std::nanf(""), 640.0f).has_value());

    const auto career1 = bootstrap::resource_names(1u, 2u);
    assert(career1.has_value());
    assert(career1->actions_plist == "p01_actions.plist");
    assert(career1->default_sprite_plists.front() == "p01_02_01_default.plist");
    assert(career1->default_sprite_plists.back() == "p01_02_09_default.plist");

    const auto career5 = bootstrap::resource_names(5u, 0u);
    assert(career5.has_value());
    assert(career5->actions_plist == "p05_actions.plist");
    assert(career5->default_sprite_plists.front() == "p05_00_01_default.plist");
    assert(career5->default_sprite_plists.back() == "p05_00_09_default.plist");

    // printf's %02 width is a minimum, not a range restriction. Keep a
    // recovered resource selector lossless instead of inventing a 0..99 bound.
    const auto wide_color = bootstrap::resource_names(3u, 123u);
    assert(wide_color.has_value());
    assert(wide_color->default_sprite_plists.front() == "p03_123_01_default.plist");

    assert(!bootstrap::resource_names(0u, 1u).has_value());
    assert(!bootstrap::resource_names(6u, 1u).has_value());
    return 0;
}
