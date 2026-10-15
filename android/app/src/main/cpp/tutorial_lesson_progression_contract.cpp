#include "tutorial_lesson_progression_contract.h"

namespace nevergone::tutorial_lesson_progression_contract {

Step step_for_counter(int lesson_counter) {
    if (lesson_counter < static_cast<int>(Step::kRun) ||
            lesson_counter > static_cast<int>(Step::kExecution)) {
        return Step::kUnknown;
    }
    return static_cast<Step>(lesson_counter);
}

const char* step_name(Step step) {
    switch (step) {
        case Step::kUnknown: return "unknown";
        case Step::kRun: return "run";
        case Step::kJump: return "jump";
        case Step::kAttack: return "attack";
        case Step::kBlock: return "block";
        case Step::kCounterAttack: return "counter-attack";
        case Step::kDodge: return "dodge";
        case Step::kAirAttack: return "air-attack";
        case Step::kFillRage: return "fill-rage";
        case Step::kExecution: return "execution";
    }
    return "unknown";
}

const char* spawn_plan_name(SpawnPlan plan) {
    switch (plan) {
        case SpawnPlan::kNone: return "none";
        case SpawnPlan::kAttackWave: return "attack-wave";
        case SpawnPlan::kBlockEnemy: return "block-enemy";
        case SpawnPlan::kAirAttackEnemy: return "air-attack-enemy";
        case SpawnPlan::kExecutionWave: return "execution-wave";
    }
    return "unknown";
}

int enemy_null_count_from_dead_flags(const bool* is_dead, std::size_t count) {
    if (count != 0u && is_dead == nullptr) return -1;
    int alive = 0;
    for (std::size_t index = 0; index < count; ++index) {
        if (!is_dead[index]) ++alive;
    }
    return alive;
}

Decision evaluate_state_callback(const Context& context) {
    Decision result;
    result.step = step_for_counter(context.lesson_counter);

    switch (result.step) {
        case Step::kRun:
            result.advance = context.meplayer_state == 0x01;
            break;
        case Step::kJump:
            if (context.meplayer_state == 0x20) {
                result.advance = true;
                result.spawn_plan = SpawnPlan::kAttackWave;
            }
            break;
        case Step::kAttack:
            break;
        case Step::kBlock:
            if (context.meplayer_state == 0x24) {
                result.advance = true;
                result.arm_counter_gate = true;
            }
            break;
        case Step::kCounterAttack: {
            const int expected_state = context.player_index == 1 ? 0x09 : 0x08;
            result.advance = context.meplayer_state == expected_state;
            break;
        }
        case Step::kDodge:
            if (context.meplayer_state == 0x11 ||
                    context.meplayer_state == 0x17 ||
                    context.meplayer_state == 0x2f ||
                    context.meplayer_state == 0x30) {
                result.advance = true;
                result.spawn_plan = SpawnPlan::kAirAttackEnemy;
                result.grant_rage_item = true;
            }
            break;
        case Step::kAirAttack:
            break;
        case Step::kFillRage:
            if (context.quick_use_pending) {
                result.advance = true;
                result.fill_super_energy = true;
            }
            break;
        case Step::kExecution:
            if (context.alive_enemy_count == 0) {
                result.advance = true;
                result.schedule_second_dialogue = true;
                result.second_dialogue_delay_seconds = 2.0f;
                result.reset_controls = true;
            }
            break;
        case Step::kUnknown:
            break;
    }

    return result;
}

Decision evaluate_poll_callback(const Context& context) {
    Decision result;
    result.step = step_for_counter(context.lesson_counter);

    switch (result.step) {
        case Step::kAttack:
            if (context.alive_enemy_count == 0 &&
                    (context.player_state_278 == 0x1b ||
                     context.player_state_278 == 0x1c)) {
                result.advance = true;
                result.spawn_plan = SpawnPlan::kBlockEnemy;
            }
            break;
        case Step::kAirAttack:
            if (context.alive_enemy_count == 0) {
                result.advance = true;
                result.spawn_plan = SpawnPlan::kExecutionWave;
            }
            break;
        default:
            break;
    }

    return result;
}

}  // namespace nevergone::tutorial_lesson_progression_contract
