#include "enemy_online_state_contract.h"

#include "enemy_hit_gate_contract.h"

namespace nevergone::enemy_online_state_contract {
namespace {

bool state_uses_beaten_gate(std::uint32_t state) noexcept {
    return state == 0x0fu || state == 0x10u || state == 0x2cu ||
        (state >= 4u && state <= 9u) || state == 0x18u;
}

bool blocked_by_action(std::uint32_t current, std::uint32_t requested) noexcept {
    return current != 0x18u || (requested > 4u && requested != 0x20u);
}

}  // namespace

Result request_state(State* state, const Request& request) noexcept {
    Result result;
    result.requested_state = request.requested_state;
    result.resolved_state = state == nullptr ? 0 : state->current_state;
    if (state == nullptr) {
        result.outcome = Outcome::kInvalidState;
        return result;
    }
    if (request.requested_state == kNoQueuedState) {
        result.outcome = Outcome::kIgnoredSentinel;
        return result;
    }
    if (state->current_state == request.requested_state) {
        result.outcome = Outcome::kUnchanged;
        return result;
    }
    if (!request.force && enemy_hit_gate_contract::is_attack_miss(
            static_cast<std::int32_t>(state->current_state),
            request.current_action_frame)) {
        result.outcome = Outcome::kAttackMiss;
        return result;
    }

    std::uint32_t next_state = request.requested_state;
    const bool apply_health_gate = state->health > 0.0f ||
        state->current_state == 0x15u || state->current_state == 0x16u;
    if (apply_health_gate) {
        if (state_uses_beaten_gate(state->current_state)) {
            if (!enemy_hit_gate_contract::check_input_state_is_beaten(
                    next_state, state->suppress_states_5_6)) {
                if (blocked_by_action(state->current_state, next_state) &&
                    !request.current_action_done) {
                    result.outcome = Outcome::kActionStillRunning;
                    return result;
                }
            } else if (state->current_state == 0x18u &&
                    ((next_state >= 5u && next_state <= 6u) ||
                     next_state == 0x0fu || next_state == 0x2cu)) {
                next_state = 0x19u;
            }
        }

        if (state->current_state == 0x1au &&
            enemy_hit_gate_contract::check_input_state_is_beaten(
                    next_state, state->suppress_states_5_6)) {
            result.outcome = Outcome::kBeatenStateLocked;
            return result;
        }
    }

    if (state->queued_state != kNoQueuedState) {
        next_state = state->queued_state;
        state->queued_state = kNoQueuedState;
    }
    state->current_state = next_state;
    result.outcome = Outcome::kApplied;
    result.resolved_state = next_state;
    result.clear_global_enemy_state_flag = next_state == 0x15u || next_state == 0x16u;
    return result;
}

}  // namespace nevergone::enemy_online_state_contract
