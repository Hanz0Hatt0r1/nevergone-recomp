#include "enemy_health_state.h"

#include "enemy_online_state_contract.h"

namespace nevergone::enemy_health_state {

bool initialize(int base_health, State* out) {
    if (out == nullptr || base_health <= 0) return false;
    State state;
    state.max_health = static_cast<float>(base_health);
    state.current_health = static_cast<float>(base_health);
    *out = state;
    return true;
}

float health_ratio_percent(const State& state) {
    if (state.max_health <= 0.0f) return 0.0f;
    return state.current_health / state.max_health * 100.0f;
}

bool is_low_health_percent(const State& state) {
    return health_ratio_percent(state) < 30.0f;
}

OnlineHealthUpdateResult apply_online_health_update(
        State* state, int update_type, int value) {
    OnlineHealthUpdateResult result;
    if (state == nullptr) return result;

    result.accepted = true;
    const float current_health = state->current_health;
    if (update_type == 1) {
        const float damage = static_cast<float>(value);
        result.physical_damage = current_health <= damage
                ? static_cast<float>(static_cast<int>(current_health))
                : damage;
        state->current_health = current_health - damage;
    } else if (update_type == 2) {
        const float new_health = static_cast<float>(value);
        state->current_health = new_health;
        if (new_health < current_health) {
            result.physical_damage =
                    static_cast<float>(static_cast<int>(current_health - new_health));
        }
    }

    if (!(state->current_health > 0.0f)) {
        state->current_health = 0.0f;
        result.death_state_requested = true;
        enemy_online_state_contract::State online_state;
        online_state.current_state = static_cast<std::uint32_t>(state->enemy_state);
        online_state.health = state->current_health;
        const auto transition = enemy_online_state_contract::request_state(
                &online_state,
                {0x15u, true, 0, true});
        result.clear_global_enemy_state_flag_requested =
                transition.clear_global_enemy_state_flag;
        if (transition.outcome == enemy_online_state_contract::Outcome::kApplied) {
            state->enemy_state = static_cast<int>(online_state.current_state);
            state->force_action_state_no_dead = false;
        }
    }
    return result;
}

void apply_beaten_damage(State* state, float damage) {
    if (state == nullptr) return;
    state->current_health -= damage;
}

void add_health(State* state, float delta) {
    if (state == nullptr) return;
    state->current_health += delta;
}

void set_dead(State* state) {
    if (state == nullptr) return;
    state->current_health = 0.0f;
}

void set_enemy_state(State* state, int enemy_state) {
    if (state == nullptr) return;
    state->enemy_state = enemy_state;
}

void set_force_action_state_no_dead(State* state, bool enabled) {
    if (state == nullptr) return;
    state->force_action_state_no_dead = enabled;
}

bool is_dead(const State& state) {
    if (!(state.current_health <= 0.0f)) return false;
    if (state.enemy_state == 0x0f || state.enemy_state == 0x2c) return false;
    return !state.force_action_state_no_dead;
}

}  // namespace nevergone::enemy_health_state
