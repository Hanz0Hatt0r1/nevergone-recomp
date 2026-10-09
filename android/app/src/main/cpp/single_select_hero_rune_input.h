#pragma once

#include <string>

namespace nevergone::single_select_hero_rune_input {

void reset();
void set_surface_size(int width, int height);
void set_rune_source_size(int tag, int source_width, int source_height);

// Android MotionEvent action values are forwarded unchanged by GameSurfaceView.
// The input consumer captures one pointer at a time and returns true whenever a
// rune press owns the current event.
bool on_touch(int action, int pointer_id, float x, float y);

// Returns the currently pressed rune tag only while the captured pointer is
// still inside its source-size hit rectangle. The GLES compositor uses this to
// choose xrfuwenfaguangNN as the transient CCMenuItemSprite selected frame.
int pressed_tag();

std::string status_report();

}  // namespace nevergone::single_select_hero_rune_input
