#pragma once

#include <cstdint>
#include <optional>

namespace nevergone::enemy_object_action_target_position {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct Inputs {
    std::int32_t enemy_state = 0;
    bool enemy_move_state = false;
    Point enemy_position_17c;
    Point player_hl_point;
    Point requested_point;
    Point enemy_origin_hl;
    std::int32_t field_2c4 = 0;
    bool argument_30 = false;
    bool argument_34 = false;
    float random_float_result = 0.0f;
};

struct Result {
    bool valid = false;
    bool wrote_target_120 = false;
    Point target_120;
    bool called_random_float = false;
    float random_first = 0.0f;
    float random_second = 0.0f;
    std::optional<bool> flip_enemy_argument;
};

// Models EnemyObject::actionTargetPos() after its native caller supplies the
// result of isEnemyMoveState() and MyRandomFloat().
Result resolve(const Inputs& inputs) noexcept;

}  // namespace nevergone::enemy_object_action_target_position
