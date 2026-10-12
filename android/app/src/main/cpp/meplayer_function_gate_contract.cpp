#include "meplayer_function_gate_contract.h"

#include <cmath>

namespace nevergone::meplayer_function_gate_contract {
namespace {

constexpr std::uint32_t kFunctionA = 7u;
constexpr std::uint32_t kFunctionB = 8u;
constexpr std::uint32_t kFunctionC = 9u;

Outcome reject(bool write_function_state = false, std::uint32_t value = 0u) {
    Outcome out{};
    out.kind = OutcomeKind::kReject;
    out.write_function_state = write_function_state;
    out.function_state_value = value;
    return out;
}

Outcome no_action(std::uint32_t function_state) {
    Outcome out{};
    out.kind = OutcomeKind::kNoAction;
    out.write_function_state = true;
    out.function_state_value = function_state;
    return out;
}

Outcome dispatch(
        std::uint32_t function_state,
        std::uint32_t action_state,
        bool force) {
    Outcome out{};
    out.kind = OutcomeKind::kDispatch;
    out.write_function_state = true;
    out.function_state_value = function_state;
    out.action_state = action_state;
    out.force = force;
    return out;
}

bool b_special_guard(std::uint32_t field_1f0, std::uint32_t field_1f8) {
    const bool first = field_1f0 == 0x12u || field_1f0 == 0x13u;
    const bool second = field_1f8 == 0x11u || field_1f8 == 0x0fu;
    return first || second;
}

}  // namespace

bool is_common_blocked_action(std::uint32_t current_action_state) {
    return current_action_state == 0x42u ||
        (current_action_state >= 0x39u && current_action_state <= 0x3cu) ||
        current_action_state == 0x3fu;
}

std::optional<Outcome> evaluate_base(const Input& input) {
    const std::uint32_t requested = input.requested_function_state;
    if (requested != kFunctionA && requested != kFunctionB && requested != kFunctionC) {
        return std::nullopt;
    }

    // MEPlayer::setFunctionState head lock used by the tutorial path. The
    // literal at ELF 0x003213e8 is 0x42c80000 == 100.0f. Any function state
    // other than 0x1c is cleared and rejected while this lock is active; base
    // A/B/C therefore all follow the clear-and-return path.
    if (input.app_mode == 0x0bu && input.lesson_step == 0x0cu &&
        std::isfinite(input.field_328) && input.field_328 >= 100.0f) {
        return reject(true, 0u);
    }

    if (is_common_blocked_action(input.current_action_state)) {
        return reject();
    }

    // Repeating the same function state is normally a no-op. Function state 8
    // is the sole exception and is allowed to re-evaluate its B branch unless
    // the current action is 0x24. A new B request is also blocked at 0x24.
    if (input.previous_function_state == requested) {
        if (requested != kFunctionB) return reject();
        if (input.current_action_state == 0x24u) return reject();
    } else if (requested == kFunctionB && input.current_action_state == 0x24u) {
        return reject();
    }

    if (requested == kFunctionA) {
        return dispatch(kFunctionA, 0x02u, false);
    }

    if (requested == kFunctionC) {
        return dispatch(kFunctionC, 0x20u, false);
    }

    // B/8 does not have a single guarded 0x11 action. The shipped branch uses
    // MEPlayer +0x1f0/+0x1f8 to choose whether 0x11 is eligible. If eligible,
    // current action states 0..9 dispatch 0x11 forced, while >9 falls back to
    // 0x1d unforced. If the special guard is absent, action 0x0c preserves the
    // newly latched function state without changing action; every other action
    // falls back to 0x1d unforced.
    if (b_special_guard(input.field_1f0, input.field_1f8)) {
        if (input.current_action_state <= 9u) {
            return dispatch(kFunctionB, 0x11u, true);
        }
        return dispatch(kFunctionB, 0x1du, false);
    }

    if (input.current_action_state == 0x0cu) {
        return no_action(kFunctionB);
    }
    return dispatch(kFunctionB, 0x1du, false);
}

}  // namespace nevergone::meplayer_function_gate_contract
