#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace nevergone::enemy_object_flip_enemy {

struct ActionSystem {
    std::int32_t type_24c = 0;
    bool active_26c = false;
};

struct Inputs {
    std::int32_t enemy_state_274 = 0;
    std::int32_t flip_value_27c = 0;
    std::uint8_t mode_22a = 0;
    bool requested_flip = false;
    std::vector<ActionSystem> actions;
};

struct ActionFlip {
    bool argument = false;
    std::optional<bool> stored_flag_16a;
};

struct Result {
    std::vector<ActionFlip> action_flips;
    std::optional<std::int32_t> requested_enemy_state;
    std::optional<bool> enemy_flip_27c;
    bool refreshed_enemy_hl_pos = false;
};

// Models EnemyObject::flipEnemy(bool) while leaving the native state setter,
// hit-point refresh, and Cocos sprite calls as explicit outputs.
Result resolve(const Inputs& inputs);

}  // namespace nevergone::enemy_object_flip_enemy
