#pragma once

#include <vector>

namespace nevergone::enemy_physical_state {

struct State {
    bool disabled = true;
    float duration = 0.0f;
    float accumulated_damage = 0.0f;
    float next_damage_threshold = 0.0f;
    int last_state_index = 0;
    int state_index = 0;
    std::vector<int> damage_percent_by_state;
};

// EnemyPhysical::physicalStateDamageWithID(): read the integer at +0x2c+4*id.
// Returns false only when the supplied clean-room state lacks that config item.
bool damage_with_id(const State& state, int id, int* out_damage);

// EnemyPhysical::updatePhysicalState(): accumulate damage until the current
// threshold is reached, then advance the configured damage-state sequence.
bool update(State* state, float damage);

// EnemyPhysical::nextState(): clear accumulated damage and advance one state
// when the physical sequence has another configured state.
bool next_state(State* state);

}  // namespace nevergone::enemy_physical_state
