#pragma once

#include <cstddef>

namespace nevergone::tutorial_lesson_progression_contract {

enum class Step : int {
    kUnknown = 0,
    kRun = 4,
    kJump = 5,
    kAttack = 6,
    kBlock = 7,
    kCounterAttack = 8,
    kDodge = 9,
    kAirAttack = 10,
    kFillRage = 11,
    kExecution = 12,
};

enum class SpawnPlan : int {
    kNone = 0,
    kAttackWave = 1,
    kBlockEnemy = 2,
    kAirAttackEnemy = 3,
    kExecutionWave = 4,
};

struct Context {
    int lesson_counter = 0;
    int meplayer_state = 0;
    int player_index = 0;
    int alive_enemy_count = 0;
    int player_state_278 = 0;
    bool quick_use_pending = false;
};

struct Decision {
    Step step = Step::kUnknown;
    bool advance = false;
    SpawnPlan spawn_plan = SpawnPlan::kNone;
    bool arm_counter_gate = false;
    bool grant_rage_item = false;
    bool fill_super_energy = false;
    bool schedule_second_dialogue = false;
    float second_dialogue_delay_seconds = 0.0f;
    bool reset_controls = false;
};

Step step_for_counter(int lesson_counter);
const char* step_name(Step step);
const char* spawn_plan_name(SpawnPlan plan);
int enemy_null_count_from_dead_flags(const bool* is_dead, std::size_t count);
Decision evaluate_state_callback(const Context& context);
Decision evaluate_poll_callback(const Context& context);

}  // namespace nevergone::tutorial_lesson_progression_contract
