#include "enemy_object_set_enemy_move.h"

namespace nevergone::enemy_object_set_enemy_move {

bool returns_before_position_update(std::uint32_t enemy_state) noexcept {
    return (enemy_state & ~0x08u) == 0x14u ||
            enemy_state == 0x2eu || enemy_state == 0x36u;
}

Result apply(State state, const Input& input) noexcept {
    Result result;
    result.state = state;
    if (returns_before_position_update(input.enemy_state)) {
        result.returned_before_position_update = true;
        return result;
    }

    result.state.field_280.x += input.movement.x;
    result.state.field_280.y += input.movement.y;

    if (input.is_enemy_move_state) {
        result.state.action_system_position = result.state.field_280;
        result.action_system_position_updated = true;
        result.complete_for_observed_path = true;

        if (input.action_system_move_flag_2b6) {
            result.state.field_118.x =
                    result.state.field_280.x + input.movement_after_scene_clamp.x;
            result.state.field_118.y =
                    result.state.field_280.y + input.movement_after_scene_clamp.y;
            result.enemy_field_118_updated = true;
        }
    }

    result.state.field_288 = result.state.field_280;
    return result;
}

}  // namespace nevergone::enemy_object_set_enemy_move
