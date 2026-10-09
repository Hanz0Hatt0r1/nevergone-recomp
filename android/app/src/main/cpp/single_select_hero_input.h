#pragma once

namespace nevergone::single_select_hero_input {

bool on_touch(int action, int pointer_id, float x, float y);

bool on_touch_for_surface(
    int action,
    int pointer_id,
    float x,
    float y,
    int surface_width,
    int surface_height);

int pressed_tag();
void reset();

}  // namespace nevergone::single_select_hero_input
