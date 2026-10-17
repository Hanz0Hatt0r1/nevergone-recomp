#pragma once

#include <array>

namespace nevergone::enemy_physical_state {

struct State {
    bool disabled = true;
    float duration = 0.0f;
    float accumulated_damage = 0.0f;
    float next_damage_threshold = 0.0f;
    int initial_state_index = 0;
    int last_state_index = 3;
    std::array<int, 4> damage_percent_by_state{{100, 100, 100, 100}};
    int state_index = 0;
};

// EnemyPhysical::initEnemyPhysical(EnemyPhysical*): copy fields +0x14..+0x3c.
void copy_state(const State& source, State* target);

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
