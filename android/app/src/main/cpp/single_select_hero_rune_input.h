#pragma once

#include <string>

namespace nevergone::single_select_hero_rune_input {

void reset();
bool on_touch(int action, int pointer_id, float x, float y);
int pressed_tag();
std::string status_report();

}  // namespace nevergone::single_select_hero_rune_input
