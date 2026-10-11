#pragma once

#include <cstdint>

namespace nevergone::game_scene_player_joystick_input {

// Recovered fixed joystick geometry from Controls::init()/CCJoyStick:
//   node position = (204, 173)
//   touch hit radius = 165
//   update radius = 300 - 10 = 290
constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kCenterX = 204.0f;
constexpr float kCenterY = 173.0f;
constexpr float kHitRadius = 165.0f;
constexpr float kUpdateRadius = 290.0f;

struct Snapshot {
    bool captured = false;
    int pointer_id = -1;
    std::uint64_t accepted_begins = 0;
    std::uint64_t accepted_moves = 0;
    float design_x = kCenterX;
    float design_y = kCenterY;
    float power = 0.0f;
    float angle_degrees = 0.0f;
};

// Android surface coordinates are top-left based. The shipped GameScene uses
// ResolutionPolicy::ExactFit at 1136x640, so this maps the full viewport
// independently on X/Y and restores Cocos' bottom-left Y axis.
bool surface_to_design(
    int surface_width,
    int surface_height,
    float surface_x,
    float surface_y,
    float* design_x,
    float* design_y);

// MotionEvent-compatible actions: DOWN=0, UP=1, MOVE=2, CANCEL=3,
// POINTER_DOWN=5, POINTER_UP=6. Returns true only when this joystick owns the
// event/pointer. MOVE samples outside the recovered 290-unit update radius are
// ignored while retaining the previous sample, matching CCJoyStick.
bool on_touch(
    int action,
    int pointer_id,
    float surface_x,
    float surface_y,
    int surface_width,
    int surface_height);

// Mirrors CCJoyStick::timerUpdate(): while captured, dispatch the latest
// recovered power/angle sample into the project-owned player control runtime.
void tick();

// Releases any captured pointer and neutralizes the joystick sample. It does
// not reset the player's retained facing; native deactivation clears Controls
// state but does not undo the sprite orientation already applied.
void cancel_capture();

Snapshot snapshot();

}  // namespace nevergone::game_scene_player_joystick_input
