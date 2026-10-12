#pragma once

#include <cmath>
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
inline std::optional<Vector> movement_value(
        std::uint32_t player_index,
        std::uint32_t action_state,
        Vector normalized_direction) {
    if (!std::isfinite(normalized_direction.x) ||
        !std::isfinite(normalized_direction.y)) return std::nullopt;

    if (action_state == kNormalMoveActionState) {
        // MEPlayer::playerMoveValue(), ELF 0x0031db04..0x0031db2a:
        // player indices 1..3 use x*8 and y*4 for action state 0.
        if (player_index >= 1u && player_index <= 3u) {
            return Vector{normalized_direction.x * 8.0f,
                          normalized_direction.y * 4.0f};
        }
        return std::nullopt;
    }

    if (action_state == kFastMoveActionState) {
        // ELF 0x0031dab8..0x0031db02: index 1 uses 20/10 while 2/3 use 16/8.
        if (player_index == 1u) {
            return Vector{normalized_direction.x * 20.0f,
                          normalized_direction.y * 10.0f};
        }
        if (player_index == 2u || player_index == 3u) {
            return Vector{normalized_direction.x * 16.0f,
                          normalized_direction.y * 8.0f};
        }
    }

    return std::nullopt;
}

}  // namespace nevergone::player_motion_contract
