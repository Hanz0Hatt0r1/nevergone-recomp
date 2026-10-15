#pragma once

namespace nevergone::enemy_health_state {

struct State {
    float max_health = 0.0f;
    float current_health = 0.0f;
    int enemy_state = 0;
    bool force_action_state_no_dead = false;
};

// EnemyObject::loadEnemyObjectFile copies GameLevelsEnemyData health into both
// +0x160 (max) and +0x164 (current). Reject non-positive config health in the
// clean-room runtime instead of creating a divide-by-zero health ratio.
bool initialize(int base_health, State* out);

// EnemyObject::enemyHealthRatio(): current / max * 100.
float health_ratio_percent(const State& state);

// EnemyObject::beaten(float, ...): current -= damage before its special-case
// clamps/state handling. This contract intentionally models only that core HP
// mutation used by the ordinary tutorial enemy path.
void apply_beaten_damage(State* state, float damage);

// EnemyObject::addHealth(float): current += delta; the original does not clamp.
void add_health(State* state, float delta);

// EnemyObject::setEnemyDead(): current health is written to 0 before isDead().
void set_dead(State* state);

void set_enemy_state(State* state, int enemy_state);
void set_force_action_state_no_dead(State* state, bool enabled);

// EnemyObject::isDead() exact recovered predicate for the retained fields:
// current <= 0, state != 0x0f/0x2c, and force-no-dead flag == false.
bool is_dead(const State& state);

}  // namespace nevergone::enemy_health_state
