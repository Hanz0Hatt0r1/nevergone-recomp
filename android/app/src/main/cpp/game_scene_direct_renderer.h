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
    std::size_t last_animated_sprite_count = 0;
    std::size_t tutorial_enemy_action_sprite_count = 0;
    std::size_t tutorial_enemy_hittable_action_count = 0;
    std::size_t tutorial_player_enemy_contact_candidates = 0;
    std::uint8_t last_min_opacity = 255u;
    float last_camera_eye_x = 0.0f;
};

void on_surface_created();
void on_surface_changed(int width, int height);

// Draw the complete currently supported type-0 GameScene sprite set. Recovered
// post-init fade loops are evaluated from the fixed-step game clock; skew-loop
// timing is tracked but visual skew stays disabled until its CCSkewTo axis is
// recovered from raw call-site evidence.
bool draw();

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::game_scene_direct_renderer
