#pragma once

namespace nevergone::character_name_input {

using DispatchFn = void (*)(int tag);

// Android-facing adapter. Returns true whenever active CharacterName owns the
// touch surface, including the transparent modal blocker behind its controls.
bool on_touch(int action, int pointer_id, float x, float y);

// GLES-independent core used by the Android adapter and host smoke tests.
bool on_touch_for_surface(
    int action,
    int pointer_id,
    float x,
    float y,
    int surface_width,
    int surface_height,
    bool active,
    DispatchFn dispatch);

int pressed_tag();
void reset();

}  // namespace nevergone::character_name_input
