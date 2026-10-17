#pragma once

#include <cstdint>

namespace nevergone::enemy_hit_gate_contract {

struct State {
    bool has_action_system = false;
    std::uint8_t allow_hit_player_244 = 0;
    std::uint32_t last_hit_counter_16c = 0;
    std::uint32_t action_hit_counter_2a4 = 0;
};

// Mirrors the original EnemyObject accessors and hit-frame guard.
inline bool allow_hit_player(const State& state) noexcept {
    return state.allow_hit_player_244 != 0;
}

inline bool is_hit_flag(const State& state) noexcept {
    return !state.has_action_system ||
        state.last_hit_counter_16c != state.action_hit_counter_2a4;
}

inline std::uint32_t temp_hit_flag_count(const State& state) noexcept {
    return state.has_action_system ? state.action_hit_counter_2a4 : 0u;
}

inline State record_hit_flag(State state) noexcept {
    if (state.has_action_system) {
        state.last_hit_counter_16c = state.action_hit_counter_2a4;
    }
    return state;
}

// EnemyObject::isBeatenState() and checkInputStateIsBeaten() share the same
// accepted state set, except that the latter receives the candidate state.
inline bool is_beaten_state(
        std::uint32_t enemy_state, bool suppress_states_5_6_22a) noexcept {
    const std::uint32_t from_state_5 = enemy_state - 5u;
    if (suppress_states_5_6_22a && from_state_5 <= 1u) {
        return false;
    }
    return from_state_5 <= 7u || enemy_state - 15u <= 2u ||
        enemy_state == 0x34u || enemy_state == 0x2cu || enemy_state == 0x2du;
}

inline bool check_input_state_is_beaten(
        std::uint32_t input_state, bool suppress_states_5_6_22a) noexcept {
    return is_beaten_state(input_state, suppress_states_5_6_22a);
}

struct ForceBeatenFlags {
    std::uint8_t field_19c = 0;
    std::uint8_t field_19d = 0;
};

// EnemyObject::setForceBeatenState() writes the same bool byte to both fields.
inline ForceBeatenFlags set_force_beaten_state(bool enabled) noexcept {
    const auto value = static_cast<std::uint8_t>(enabled);
    return {value, value};
}

// EnemyObject::isEnemyMoveState() has fixed moving-state codes, then consults
// EnemyActionsSystem +0x2b6 for all other states.
inline bool is_enemy_move_state(
        std::uint32_t enemy_state,
        bool has_action_system,
        std::uint8_t action_move_state_2b6) noexcept {
    const std::uint32_t from_state_2a = enemy_state - 0x2au;
    if (enemy_state < 2u || enemy_state == 0x1bu || from_state_2a <= 1u ||
            enemy_state == 0x35u) {
        return true;
    }
    return has_action_system && action_move_state_2b6 != 0;
}

// EnemyObject::isAttackMiss() compares action-system frame +0x294 with a
// state-specific cutoff selected from EnemyObject +0x274.
inline bool is_attack_miss(
        std::int32_t enemy_state_274, std::int32_t current_frame_294) noexcept {
    std::int32_t frame_cutoff = 0;
    switch (enemy_state_274) {
        case 0x07:
        case 0x08:
            frame_cutoff = 4;
            break;
        case 0x09:
        case 0x0a:
            frame_cutoff = 10;
            break;
        case 0x0b:
            frame_cutoff = 15;
            break;
        case 0x0c:
        case 0x11:
        case 0x2d:
            frame_cutoff = 21;
            break;
        default:
            return false;
    }
    return current_frame_294 > frame_cutoff;
}

}  // namespace nevergone::enemy_hit_gate_contract
