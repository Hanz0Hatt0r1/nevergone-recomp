#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "game_scene_action_input.h"
#include "game_scene_action_layout.h"

namespace nevergone::game_scene_action_surface_input {

struct Snapshot {
    std::array<game_scene_action_layout::SpriteBounds, game_scene_action_layout::kButtonCount>
        sprite_bounds{};
    std::array<bool, game_scene_action_layout::kButtonCount> pressed{};
    std::uint64_t bounds_revision = 0;
    std::uint64_t accepted_begins = 0;
    std::uint64_t accepted_ends = 0;
    std::uint64_t cancelled_captures = 0;
    std::uint64_t taps = 0;
    int captured_pointer_count = 0;
};

void reset();

// Slot order is the recovered Controls::init() construction order A, B, C.
bool set_sprite_bounds(int slot, int width, int height);
void clear_sprite_bounds();

// Android MotionEvent action values are consumed directly. Surface coordinates
// are converted through the same 1136x640 design-space mapping as the recovered
// joystick path before the original ControlsButton bounding-box classifier runs.
bool on_touch(
    int action,
    int pointer_id,
    float surface_x,
    float surface_y,
    int surface_width,
    int surface_height);

void cancel_all_captures();
Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::game_scene_action_surface_input
