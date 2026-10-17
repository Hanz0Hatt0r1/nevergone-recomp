#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace nevergone::game_scene_action_compositor {

void on_surface_created();
void on_surface_changed(int width, int height);

// Slots follow game_scene_action_layout construction order A, B, C.
// selected=false uploads ab/db/jb; selected=true uploads *_s variants.
bool upload_sprite(
    int slot,
    bool selected,
    int width,
    int height,
    const std::uint32_t* argb_pixels,
    std::size_t pixel_count);

bool upload_quick_use_sprite(
    int slot,
    int width,
    int height,
    const std::uint32_t* argb_pixels,
    std::size_t pixel_count);

void clear_sprites();
bool draw();
std::string status_report();

}  // namespace nevergone::game_scene_action_compositor
