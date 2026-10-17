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
constexpr std::uint32_t kBaseAttackActionState = 0x02u;
constexpr std::uint32_t kWalkAttackActionState = 0x10u;
constexpr std::uint32_t kRunAttackActionState = 0x14u;
constexpr std::uint32_t kInitialIdleActionState = 0x1bu;
constexpr std::uint32_t kBaselineBActionState = 0x1du;
constexpr std::uint32_t kHeldBActionState = 0x1eu;
constexpr std::uint32_t kTutorialJumpActionState = 0x20u;

// Evidence-backed MEPlayer::playerMoveValue() subset. The input is the
// normalized Controls +0x168 joystick direction, not joystick power. Current
// player action state 0 and 1 use direct input; initial idle 0x1b is stationary.
// States 2/0x10/0x14 consume the recovered action-frame move accumulator.
// Other action states use additional player fields and are not synthesized here.
inline std::optional<Vector> movement_value(
        std::uint32_t player_index,
        std::uint32_t action_state,
        Vector normalized_direction,
        float action_frame_move_value_x = 0.0f) {
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

    if (action_state == kBaseAttackActionState ||
        action_state == kWalkAttackActionState ||
        action_state == kRunAttackActionState) {
        if (!std::isfinite(action_frame_move_value_x)) return std::nullopt;
        return Vector{action_frame_move_value_x, 0.0f};
    }

    // These supported live actions leave playerMoveValue()'s zero-initialized
    // result unchanged. Jump is initiated from idle, so its recovered +0x3c5
    // direction flag is clear; other jump entry paths remain unsupported.
    if (action_state == kInitialIdleActionState ||
        action_state == kBaselineBActionState ||
        action_state == kHeldBActionState ||
        action_state == kTutorialJumpActionState) {
        return Vector{};
    }

    return std::nullopt;
}

inline bool uses_direct_input(std::uint32_t action_state) noexcept {
    return action_state == kNormalMoveActionState ||
        action_state == kFastMoveActionState;
}

inline bool uses_action_frame_move_value(std::uint32_t action_state) noexcept {
    return action_state == kBaseAttackActionState ||
        action_state == kWalkAttackActionState ||
        action_state == kRunAttackActionState;
}

}  // namespace nevergone::player_motion_contract
