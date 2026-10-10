#pragma once

#include <cstddef>
#include <cstdint>

namespace nevergone::game_scene_direct_sprite_renderer {

struct Snapshot {
    bool program_ready = false;
    std::uint64_t draw_attempt_count = 0;
    std::size_t last_drawn_sprite_count = 0;
    std::uint64_t last_texture_revision = 0;
};

void on_surface_created();
std::size_t draw();
Snapshot snapshot();

}  // namespace nevergone::game_scene_direct_sprite_renderer
