#pragma once

namespace nevergone::single_select_hero_input {

// Returns true when the active SingleSelectHero route owns the event. Android
// MotionEvent action integers are passed through unchanged.
bool on_touch(int action, int pointer_id, float x, float y);

// Pure surface-aware entry used by the Android wrapper and host smoke tests.
bool on_touch_for_surface(
    int action,
    int pointer_id,
    float x,
    float y,
    int surface_width,
    int surface_height);

// Pressed menu-item tag, or 0 when no rune is visibly pressed. The renderer can
// consume this later without coupling touch state to GLES objects.
int pressed_tag();
void reset();

}  // namespace nevergone::single_select_hero_input
