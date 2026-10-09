#pragma once

#include <string>

namespace nevergone::choose_hero_action_control_compositor {

void draw();
bool on_touch(int action, int pointer_id, float surface_x, float surface_y);
std::string status_report();

}  // namespace nevergone::choose_hero_action_control_compositor
