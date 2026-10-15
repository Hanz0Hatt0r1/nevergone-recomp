#include <cassert>
#include <cmath>

#include "../android/app/src/main/cpp/tutorial_lesson_progression_contract.h"

using nevergone::tutorial_lesson_progression_contract::Context;
using nevergone::tutorial_lesson_progression_contract::SpawnPlan;
using nevergone::tutorial_lesson_progression_contract::Step;
using nevergone::tutorial_lesson_progression_contract::enemy_null_count_from_dead_flags;
using nevergone::tutorial_lesson_progression_contract::evaluate_poll_callback;
using nevergone::tutorial_lesson_progression_contract::evaluate_state_callback;
using nevergone::tutorial_lesson_progression_contract::step_for_counter;

int main() {
    assert(step_for_counter(3) == Step::kUnknown);
    assert(step_for_counter(4) == Step::kRun);
    assert(step_for_counter(5) == Step::kJump);
    assert(step_for_counter(6) == Step::kAttack);
    assert(step_for_counter(7) == Step::kBlock);
    assert(step_for_counter(8) == Step::kCounterAttack);
    assert(step_for_counter(9) == Step::kDodge);
    assert(step_for_counter(10) == Step::kAirAttack);
    assert(step_for_counter(11) == Step::kFillRage);
    assert(step_for_counter(12) == Step::kExecution);
    assert(step_for_counter(13) == Step::kUnknown);

    const bool dead_flags[] = {false, true, false, true};
    assert(enemy_null_count_from_dead_flags(dead_flags, 4) == 2);
    assert(enemy_null_count_from_dead_flags(nullptr, 0) == 0);
    assert(enemy_null_count_from_dead_flags(nullptr, 1) == -1);

    Context context;
    context.lesson_counter = 4;
    context.meplayer_state = 1;
    assert(evaluate_state_callback(context).advance);
    context.meplayer_state = 2;
    assert(!evaluate_state_callback(context).advance);

    context = {};
    context.lesson_counter = 5;
    context.meplayer_state = 0x20;
    auto decision = evaluate_state_callback(context);
    assert(decision.advance);
    assert(decision.spawn_plan == SpawnPlan::kAttackWave);

    context = {};
    context.lesson_counter = 6;
    context.alive_enemy_count = 0;
    context.player_state_278 = 0x1b;
    decision = evaluate_poll_callback(context);
    assert(decision.advance);
    assert(decision.spawn_plan == SpawnPlan::kBlockEnemy);
    context.player_state_278 = 0x1c;
    assert(evaluate_poll_callback(context).advance);
    context.player_state_278 = 0x1a;
    assert(!evaluate_poll_callback(context).advance);
    context.player_state_278 = 0x1b;
    context.alive_enemy_count = 1;
    assert(!evaluate_poll_callback(context).advance);

    context = {};
    context.lesson_counter = 7;
    context.meplayer_state = 0x24;
    decision = evaluate_state_callback(context);
    assert(decision.advance);
    assert(decision.arm_counter_gate);

    context = {};
    context.lesson_counter = 8;
    context.player_index = 1;
    context.meplayer_state = 0x09;
    assert(evaluate_state_callback(context).advance);
    context.meplayer_state = 0x08;
    assert(!evaluate_state_callback(context).advance);
    context.player_index = 2;
    assert(evaluate_state_callback(context).advance);

    for (int state : {0x11, 0x17, 0x2f, 0x30}) {
        context = {};
        context.lesson_counter = 9;
        context.meplayer_state = state;
        decision = evaluate_state_callback(context);
        assert(decision.advance);
        assert(decision.spawn_plan == SpawnPlan::kAirAttackEnemy);
        assert(decision.grant_rage_item);
    }
    context.meplayer_state = 0x10;
    assert(!evaluate_state_callback(context).advance);

    context = {};
    context.lesson_counter = 10;
    context.alive_enemy_count = 0;
    decision = evaluate_poll_callback(context);
    assert(decision.advance);
    assert(decision.spawn_plan == SpawnPlan::kExecutionWave);
    context.alive_enemy_count = 1;
    assert(!evaluate_poll_callback(context).advance);

    context = {};
    context.lesson_counter = 11;
    context.quick_use_pending = true;
    decision = evaluate_state_callback(context);
    assert(decision.advance);
    assert(decision.fill_super_energy);
    context.quick_use_pending = false;
    assert(!evaluate_state_callback(context).advance);

    context = {};
    context.lesson_counter = 12;
    context.alive_enemy_count = 0;
    decision = evaluate_state_callback(context);
    assert(decision.advance);
    assert(decision.schedule_second_dialogue);
    assert(std::fabs(decision.second_dialogue_delay_seconds - 2.0f) < 0.0001f);
    assert(decision.reset_controls);
    context.alive_enemy_count = 1;
    assert(!evaluate_state_callback(context).advance);

    return 0;
}
