#pragma once

namespace nevergone::single_select_hero_pointer_router {

void set_surface_size(int width, int height);
void reset();

// Returns true when the SingleSelectHero rune menu owns the MotionEvent.
bool on_touch(int action, int pointer_id, float surface_x, float surface_y);

}  // namespace nevergone::single_select_hero_pointer_router
