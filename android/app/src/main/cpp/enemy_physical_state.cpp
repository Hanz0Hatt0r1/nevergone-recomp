#include "enemy_physical_state.h"

#include <cmath>

namespace nevergone::enemy_physical_state {
namespace {

bool percentage_for_id(const State& state, int id, int* out_percentage) {
    if (id < 0 || out_percentage == nullptr ||
        static_cast<std::size_t>(id) >= state.damage_percent_by_state.size()) {
        return false;
    }
    *out_percentage = state.damage_percent_by_state[static_cast<std::size_t>(id)];
    return true;
}

void set_threshold(State* state, int percentage) {
    state->next_damage_threshold =
            (static_cast<float>(percentage) / 100.0f) * state->duration;
}

}  // namespace

void copy_state(const State& source, State* target) {
    if (target == nullptr) return;
    *target = source;
}

bool load_config(const Config& config, State* state) {
    if (state == nullptr) return false;
    if (!config.no_physical &&
        (config.start_physical < 0 || config.start_physical >= 4 ||
         config.max_physical < 0 || config.max_physical >= 4)) {
        return false;
    }

    state->disabled = config.no_physical;
    if (config.no_physical) {
        state->state_index = 0;
        return true;
    }
    state->initial_state_index = config.start_physical;
    state->last_state_index = config.max_physical;
    state->damage_percent_by_state = config.damage_percent;
    state->state_index = config.start_physical;
    int percentage = 0;
    if (!percentage_for_id(*state, state->state_index, &percentage)) return false;
    set_threshold(state, percentage);
    return true;
}

bool initialize(const Config& config, int duration, State* out) {
    if (out == nullptr) return false;
    State state = *out;
    state.duration = static_cast<float>(duration);
    if (!load_config(config, &state)) return false;
    *out = state;
    return true;
}

bool damage_with_id(const State& state, int id, int* out_damage) {
    return percentage_for_id(state, id, out_damage);
}

bool update(State* state, float damage) {
    if (state == nullptr || state->disabled || state->state_index == 2) return false;

    state->accumulated_damage += damage;
    if (std::isnan(state->accumulated_damage) ||
        std::isnan(state->next_damage_threshold)) {
        return false;
    }
    if (state->accumulated_damage < state->next_damage_threshold) return false;
    state->accumulated_damage = 0.0f;
    if (state->state_index > state->last_state_index) return false;

    const int next_index = state->state_index == state->last_state_index
            ? 0
            : state->state_index + 1;
    int percentage = 0;
    if (!percentage_for_id(*state, next_index, &percentage)) return false;

    state->state_index = next_index;
    set_threshold(state, percentage);
    return true;
}

bool next_state(State* state) {
    if (state == nullptr || state->disabled) return false;

    state->accumulated_damage = 0.0f;
    if (state->state_index >= state->last_state_index) return false;

    const int next_index = state->state_index + 1;
    int percentage = 0;
    if (!percentage_for_id(*state, next_index, &percentage)) return false;

    state->state_index = next_index;
    set_threshold(state, percentage);
    return true;
}

}  // namespace nevergone::enemy_physical_state
