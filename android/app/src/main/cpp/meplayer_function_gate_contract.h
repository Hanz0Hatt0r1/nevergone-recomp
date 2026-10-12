#pragma once

#include <cstdint>
#include <optional>

namespace nevergone::meplayer_function_gate_contract {

enum class OutcomeKind : std::uint8_t {
    kReject = 0,
    kNoAction = 1,
    kDispatch = 2,
};

struct Input {
    std::uint32_t requested_function_state = 0;
    std::uint32_t previous_function_state = 0;
    std::uint32_t current_action_state = 0;

    // Shipped MEPlayer fields used by the B/8 branch.
    std::uint32_t field_1f0 = 0;
    std::uint32_t field_1f8 = 0;

    // The cp_lesson lock at the head of MEPlayer::setFunctionState().
    std::uint32_t app_mode = 0;
    std::uint32_t lesson_step = 0;
    float field_328 = 0.0f;
};

struct Outcome {
    OutcomeKind kind = OutcomeKind::kReject;

    // Whether the original stores the requested value to MEPlayer +0x1f4 before
    // reaching this branch. A rejected head gate leaves the previous value alone,
    // except the tutorial lock which explicitly clears it to zero.
    bool write_function_state = false;
    std::uint32_t function_state_value = 0;

    std::uint32_t action_state = 0;
    bool force = false;
};

// Reconstructs the exact common gate plus the already-proven base A/B/C
// dispatch branches of MEPlayer::setFunctionState(CONTROLSSTATE). Non-base
// function states are intentionally left unsupported until their branches are
// modeled individually.
std::optional<Outcome> evaluate_base(const Input& input);

bool is_common_blocked_action(std::uint32_t current_action_state);

}  // namespace nevergone::meplayer_function_gate_contract
