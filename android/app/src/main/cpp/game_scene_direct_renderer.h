#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace nevergone::game_scene_direct_renderer {

struct Snapshot {
    bool shader_ready = false;
    int surface_width = 0;
    int surface_height = 0;
    std::uint64_t draw_attempt_count = 0;
    std::uint64_t drawn_frame_count = 0;
    std::size_t last_drawn_sprite_count = 0;
};

void on_surface_created();
void on_surface_changed(int width, int height);

// Draw the complete currently supported static type-0 GameScene sprite set:
// direct-file resources plus spriteFrameByName resources reconstructed from the
// imported atlas metadata/pixels. Returns true only when at least one textured
// quad was submitted for the live scene revision.
bool draw();

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::game_scene_direct_renderer
