#pragma once

#include <cstdint>
#include <string>

namespace nevergone::game_scene_player_motion_state {

struct Snapshot {
    bool ready = false;
    std::uint64_t generation = 0;
    std::uint64_t spawn_generation = 0;
    std::uint32_t player_index = 0;
    std::uint64_t applied_steps = 0;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

void reset();

// Binds the live movement cursor to one recovered player spawn. A new spawn
// generation clears accumulated motion. Player indices outside the statically
// proven playerMoveValue normal-movement domain (1..3) fail closed.
bool sync_spawn(std::uint64_t spawn_generation, std::uint32_t player_index);

// Applies one normal locomotion tick from the normalized Controls joystick
// direction. The actual per-axis scales come from player_motion_contract.
bool apply_normal_direction(float direction_x, float direction_y);

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::game_scene_player_motion_state
