#pragma once

#include <cstdint>
#include <string>

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
    float center_x = kCenterX;
    float center_y = kCenterY;
    float design_x = kCenterX;
    float design_y = kCenterY;
    float power = 0.0f;
    float angle_degrees = 0.0f;

    // Bounded Controls::comboJoyStickUpdate() reconstruction. The retained
    // state/timer intentionally covers only the horizontal normal/fast branch;
    // vertical and cross-direction combo states remain unmodeled.
    std::uint32_t direction_combo_state = 0u;
    bool direction_combo_released = true;
    std::uint64_t direction_combo_started_tick = 0u;
    std::uint32_t movement_action_state = 0u;
    std::uint64_t fast_reactivation_count = 0u;
    std::uint64_t enemy_collision_checks = 0u;
    std::uint64_t enemy_collision_count = 0u;
    std::uint64_t enemy_collision_correction_count = 0u;
    int last_enemy_collision_slot = -1;
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

bool set_center(float design_x, float design_y);

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

// Mirrors the recovered fixed-joystick delivery on the project logical clock:
// primary horizontal facing, the bounded same-direction double-tap fast combo,
// and the corresponding playerMoveValue() movement state are advanced together.
void tick();

// Keep bounded per-render and raw-touch history for original-client comparison.
void record_frame_sample(bool controls_allowed);
void record_input_event(int action, int pointer_id, float surface_x, float surface_y);

// Releases the live pointer and neutralizes the joystick sample while retaining
// facing and the native short-lived direction-combo window needed for a second
// same-direction tap.
void cancel_capture();

// Scene/player teardown additionally clears reconstructed control/combo/motion state.
void reset();

Snapshot snapshot();

// Resource-name-free runtime diagnostics for retained Android input evidence.
std::string status_report();

}  // namespace nevergone::game_scene_player_joystick_input
