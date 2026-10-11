#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace nevergone::player_control_contract {

constexpr float kHorizontalActivityThreshold = 0.05f;
constexpr float kNegativeBoundaryDegrees = -90.0f;
constexpr float kPositiveBoundaryDegrees = 90.0f;
constexpr float kNegativeWrapDegrees = -180.0f;

enum class PrimaryControlState : std::int32_t {
    kState12 = 0x12,
    kState13 = 0x13,
};

enum class TickTarget : std::uint8_t {
    kDirCombo = 0,
    kDirection,
    kFunction,
    kPrimary,
};

struct TickDispatch {
    TickTarget target = TickTarget::kDirCombo;
    std::int32_t state = 0;
};

// Mirrors only the bounded horizontal-state assignment observed in
// Controls::onCCJoyStickUpdate. It deliberately does not infer movement speed,
// acceleration, or a displacement from joystick power.
std::optional<PrimaryControlState> horizontal_primary_state(
    float activity_metric,
    float angle_degrees);

// Recovered MEPlayer::setControlsState()/filpPlayer() facing contract:
// state 0x12 requests flip-X=true and state 0x13 requests flip-X=false.
// The optional return keeps invalid enum casts fail-closed.
std::optional<bool> facing_flip_x(PrimaryControlState state);

// Mirrors the recovered GameScene::tick dispatch order into the player path:
// dir-combo, direction, function, then primary controls.
std::array<TickDispatch, 4> tick_dispatches(
    std::int32_t dir_combo,
    std::int32_t direction,
    std::int32_t function,
    std::int32_t primary);

const char* state_name(PrimaryControlState state);
const char* target_name(TickTarget target);

}  // namespace nevergone::player_control_contract
