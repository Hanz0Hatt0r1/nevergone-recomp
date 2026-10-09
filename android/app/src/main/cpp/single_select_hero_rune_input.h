#pragma once

#include <string>

namespace nevergone::single_select_hero_rune_input {

void reset();
void set_surface_size(int width, int height);
void set_rune_source_size(int tag, int source_width, int source_height);

// Android MotionEvent values are forwarded unchanged. One pointer is captured
// at a time, mirroring the single CCMenuItemSprite press that owns the gesture.
bool on_touch(int action, int pointer_id, float x, float y);

// Non-zero only while the captured pointer remains inside its original rune.
// The GLES compositor uses this to draw xrfuwenfaguangNN transiently.
int pressed_tag();

std::string status_report();

}  // namespace nevergone::single_select_hero_rune_input
