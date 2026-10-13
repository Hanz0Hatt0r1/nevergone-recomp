#include <cassert>
#include <cmath>

#include "scene_actions_klhuo_fallback_bounds.h"

bool near(float a, float b) {
    return std::fabs(a - b) < 0.001f;
}

int main() {
    nevergone::game_levels_scene_prefix::ObjectRecord record;
    record.string_value = "klhuo-1.png";
    record.first_i32 = 0;
    record.first_point_x = 100.0f;
    record.first_point_y = 200.0f;
    record.middle_float = 0.0f;
    record.second_point_x = 1.0f;
    record.second_point_y = 1.0f;
    record.first_bool = false;

    auto bounds = nevergone::scene_actions_klhuo_fallback_bounds::build(record);
    assert(bounds.has_value());
    assert(near(bounds->x, 90.0f));
    assert(near(bounds->y, 190.0f));
    assert(near(bounds->width, 20.0f));
    assert(near(bounds->height, 20.0f));

    record.middle_float = 90.0f;
    record.second_point_x = 2.0f;
    record.second_point_y = 0.5f;
    bounds = nevergone::scene_actions_klhuo_fallback_bounds::build(record);
    assert(bounds.has_value());
    assert(near(bounds->width, 10.0f));
    assert(near(bounds->height, 40.0f));

    record.string_value = "gkshu01.png";
    assert(!nevergone::scene_actions_klhuo_fallback_bounds::build(record).has_value());
    return 0;
}
