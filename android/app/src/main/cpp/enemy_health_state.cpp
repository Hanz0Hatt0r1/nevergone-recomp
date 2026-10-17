#include "enemy_health_state.h"

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
