#pragma once

#include <string>

namespace nevergone::server_selection_compositor {

void on_surface_created();
void on_surface_changed(int width, int height);
void draw();

// Returns true when the reconstructed server-selection layer owned the event.
bool on_touch(int action, int pointer_id, float surface_x, float surface_y);

bool active();
std::string status_report();

}  // namespace nevergone::server_selection_compositor
