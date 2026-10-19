#pragma once

#include <cstdint>

namespace nevergone::enemy_object_set_enemy_move {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct State {
    Point field_280;
    Point field_288;
    Point field_118;
    Point action_system_position;
};

struct Input {
    std::uint32_t enemy_state = 0;
    bool is_enemy_move_state = false;
    bool action_system_move_flag_2b6 = false;
    Point movement;
    Point movement_after_scene_clamp;
};

struct Result {
    State state;
    bool returned_before_position_update = false;
    bool action_system_position_updated = false;
    bool enemy_field_118_updated = false;
    // The non-move-state branch also depends on event-scene objects and the
    // setter's boolean argument; this bounded model does not claim that path.
    bool complete_for_observed_path = false;
};

bool returns_before_position_update(std::uint32_t enemy_state) noexcept;
Result apply(State state, const Input& input) noexcept;

}  // namespace nevergone::enemy_object_set_enemy_move
