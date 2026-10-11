#include "player_motion_contract.h"

#include <cmath>

namespace nevergone::player_motion_contract {
namespace {

bool finite(Vector value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

Vector scaled(Vector value, float x_scale, float y_scale) {
    return {value.x * x_scale, value.y * y_scale};
}

}  // namespace

std::optional<Vector> movement_value(
        std::uint32_t player_index,
        std::uint32_t action_state,
        Vector normalized_direction) {
    if (!finite(normalized_direction)) return std::nullopt;

    if (action_state == kNormalMoveActionState) {
        // MEPlayer::playerMoveValue(), ELF 0x0031db04..0x0031db2a:
        // player indices 1..3 all use x*8 and y*4 for action state 0.
        if (player_index >= 1u && player_index <= 3u) {
            return scaled(normalized_direction, 8.0f, 4.0f);
        }
        return std::nullopt;
    }

    if (action_state == kFastMoveActionState) {
        // ELF 0x0031dab8..0x0031db02. Player index 1 uses 20/10 while
        // indices 2 and 3 use 16/8. This path is retained as evidence but is
        // not selected by the first reconstructed joystick locomotion step.
        if (player_index == 1u) return scaled(normalized_direction, 20.0f, 10.0f);
        if (player_index == 2u || player_index == 3u) {
            return scaled(normalized_direction, 16.0f, 8.0f);
        }
    }

    return std::nullopt;
}

}  // namespace nevergone::player_motion_contract
