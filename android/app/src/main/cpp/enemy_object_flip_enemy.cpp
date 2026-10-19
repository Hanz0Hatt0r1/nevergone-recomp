#include "enemy_object_flip_enemy.h"

namespace nevergone::enemy_object_flip_enemy {

Result resolve(const Inputs& inputs) {
    Result result;
    if (inputs.enemy_state_274 == 0x1a || inputs.enemy_state_274 == 0x2e ||
        (inputs.flip_value_27c == 1 && inputs.requested_flip) ||
        (inputs.flip_value_27c == 0 && !inputs.requested_flip)) {
        return result;
    }

    if (inputs.mode_22a == 0u && inputs.actions.empty()) return result;

    result.action_flips.reserve(inputs.actions.size());
    for (const ActionSystem& action : inputs.actions) {
        bool flip = inputs.requested_flip;
        if (inputs.mode_22a != 0u &&
            (action.type_24c == 5 || action.type_24c == 6)) {
            flip = inputs.flip_value_27c == 1;
        }
        result.action_flips.push_back({
                flip,
                action.active_26c ? std::optional<bool>(flip) : std::nullopt});
    }

    if (inputs.mode_22a == 0u) return result;

    if (!inputs.actions.empty()) {
        result.requested_enemy_state =
                inputs.enemy_state_274 == 1 || inputs.enemy_state_274 == 0x1e
                ? 6
                : 5;
    }
    result.enemy_flip_27c = inputs.requested_flip;
    result.refreshed_enemy_hl_pos = true;
    return result;
}

}  // namespace nevergone::enemy_object_flip_enemy
