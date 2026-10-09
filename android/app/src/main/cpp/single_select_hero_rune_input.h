#pragma once

#include <string>

namespace nevergone::single_select_hero_rune_input {

void reset();

// Android MotionEvent action values are forwarded unchanged by GameSurfaceView.
// One pointer is captured at a time. Outside OpeningDialogue this consumer
// immediately yields to the rest of the existing native touch router.
bool on_touch(int action, int pointer_id, float x, float y);

int pressed_tag();
std::string status_report();

}  // namespace nevergone::single_select_hero_rune_input
