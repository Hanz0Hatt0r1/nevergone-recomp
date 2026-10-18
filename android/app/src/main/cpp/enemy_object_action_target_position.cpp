#include "enemy_object_action_target_position.h"

#include <cmath>
#include <limits>

namespace nevergone::enemy_object_action_target_position {
namespace {

bool is_finite(Point point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

bool truncated_distance(Point lhs, Point rhs, std::int32_t* output) {
    const float dx = lhs.x - rhs.x;
    const float dy = lhs.y - rhs.y;
    const float distance = std::sqrt(dx * dx + dy * dy);
    if (!std::isfinite(distance) || distance < 0.0f ||
        static_cast<double>(distance) >
                static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
        return false;
    }
    *output = static_cast<std::int32_t>(distance);
    return true;
}

bool skips_direction_update(std::int32_t state) {
    return state == 0x2b || state == 0x1b;
}

}  // namespace

Result resolve(const Inputs& inputs) noexcept {
    Result result;

    if (!is_finite(inputs.requested_point)) return result;

    if (!inputs.enemy_move_state) {
        if (!is_finite(inputs.enemy_origin_hl)) return result;

        std::int32_t distance = 0;
        if (!truncated_distance(
                    inputs.enemy_origin_hl, inputs.requested_point, &distance)) {
            return result;
        }

        result.valid = true;
        if (distance <= 80) return result;

        result.flip_enemy_argument =
                inputs.enemy_origin_hl.x <= inputs.requested_point.x;
        return result;
    }

    if (!is_finite(inputs.enemy_position_17c) ||
        !is_finite(inputs.player_hl_point)) {
        return result;
    }

    std::int32_t distance = 0;
    if (!truncated_distance(
                inputs.enemy_position_17c, inputs.player_hl_point, &distance)) {
        return result;
    }

    const std::uint32_t distance_minus_60 =
            static_cast<std::uint32_t>(distance) - 60u;
    const bool distance_condition = distance_minus_60 > 4036u;
    const bool randomize_x = inputs.argument_30 ||
            (distance_condition && !inputs.argument_34);
    result.valid = true;
    result.wrote_target_120 = true;
    result.target_120.y = inputs.requested_point.y;

    if (randomize_x) {
        const std::int32_t half_field_2c4 = inputs.field_2c4 / 2;
        if (inputs.enemy_position_17c.x > static_cast<float>(half_field_2c4)) {
            result.random_first = 200.0f;
            result.random_second = inputs.enemy_position_17c.x;
            result.flip_enemy_argument = false;
        } else {
            result.random_first = inputs.enemy_position_17c.x;
            result.random_second = static_cast<float>(inputs.field_2c4);
            result.flip_enemy_argument = true;
        }
        result.called_random_float = true;
        result.target_120.x = inputs.random_float_result;
        return result;
    }

    result.target_120.x = inputs.requested_point.x;
    if (!skips_direction_update(inputs.enemy_state)) {
        result.flip_enemy_argument =
                inputs.enemy_position_17c.x <= inputs.requested_point.x;
    }
    return result;
}

}  // namespace nevergone::enemy_object_action_target_position
