#pragma once

namespace nevergone::single_select_hero_touch_bridge {

void reset_assets();
bool configure_hitbox(
    int tag,
    int width,
    int height,
    int left,
    int top,
    int source_width,
    int source_height);
void set_surface_size(int width, int height);
bool on_touch(int action, int pointer_id, float x, float y);

}  // namespace nevergone::single_select_hero_touch_bridge
