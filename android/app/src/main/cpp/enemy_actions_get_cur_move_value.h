#pragma once

namespace nevergone::enemy_actions_get_cur_move_value {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct State {
    Point pending;
    Point accumulated;
};

struct Result {
    State state;
    Point movement;
};

// Mirrors EnemyActionsSystem::GetCurMoveValue(): return +0x180, clear it,
// then add the returned point to the cumulative +0x270 value.
Result consume(State state) noexcept;

}  // namespace nevergone::enemy_actions_get_cur_move_value
