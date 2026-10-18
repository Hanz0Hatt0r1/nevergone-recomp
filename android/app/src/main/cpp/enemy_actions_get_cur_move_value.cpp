#include "enemy_actions_get_cur_move_value.h"

namespace nevergone::enemy_actions_get_cur_move_value {

Result consume(State state) noexcept {
    const Point movement = state.pending;
    state.pending = {};
    state.accumulated.x += movement.x;
    state.accumulated.y += movement.y;
    return {state, movement};
}

}  // namespace nevergone::enemy_actions_get_cur_move_value
