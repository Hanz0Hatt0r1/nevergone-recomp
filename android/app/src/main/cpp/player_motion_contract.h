#pragma once

#include <cstdint>
#include <optional>

namespace nevergone::player_motion_contract {

struct Vector {
    float x = 0.0f;
    float y = 0.0f;
};

constexpr std::uint32_t kNormalMoveActionState = 0x00u;
constexpr std::uint32_t kFastMoveActionState = 0x01u;

// Evidence-backed MEPlayer::playerMoveValue() subset. The input is the
// normalized Controls +0x168 joystick direction, not joystick power. Only the
// action/player-index combinations proven by the shipped ARMv7 implementation
// are exposed; every other combination fails closed.
std::optional<Vector> movement_value(
        std::uint32_t player_index,
        std::uint32_t action_state,
        Vector normalized_direction);

}  // namespace nevergone::player_motion_contract
