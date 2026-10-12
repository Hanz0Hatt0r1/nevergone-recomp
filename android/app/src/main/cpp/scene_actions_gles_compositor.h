#pragma once

#include <cstdint>
#include <string>

#include "scene_action_initial_frame_pixel_store.h"
#include "scene_actions_render_frame_plan.h"

namespace nevergone::scene_actions_gles_compositor {

void on_surface_created();
void on_surface_changed(int width, int height);

bool submit(
        const scene_actions_render_frame_plan::Plan& plan,
        const scene_action_initial_frame_pixel_store::Asset& asset,
        float parent_x,
        float parent_y,
        int child_z_order);

void clear(scene_action_pair_document_store::Slot slot);
void reset();
bool draw();
std::string status_report();

}  // namespace nevergone::scene_actions_gles_compositor
