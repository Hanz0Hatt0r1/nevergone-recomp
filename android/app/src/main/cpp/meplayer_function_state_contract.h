#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace nevergone::meplayer_function_state_contract {

constexpr int kNone = 0;
constexpr int kButtonA = 7;
constexpr int kButtonB = 8;
constexpr int kButtonC = 9;

constexpr std::uint32_t kInitialActionState = 0x1bu;
constexpr std::uint32_t kButtonABaseActionState = 0x02u;
constexpr std::uint32_t kButtonBGuardedActionState = 0x11u;
constexpr std::uint32_t kButtonBFallbackActionState = 0x1du;
constexpr std::uint32_t kButtonCJumpActionState = 0x20u;

struct Transition {
    std::uint32_t action_state = 0;
    bool blend_or_restart = false;
};

// Returns only transitions that are unconditional for the recovered baseline
// function states. B is intentionally omitted here because setFunctionState(8)
// branches on live MEPlayer state/history.
std::optional<Transition> unconditional_transition(int function_state);

// Exact recovered baseline for pressing B while the player is in the project's
// current initial action state 0x1b, with no prior guarded-action history.
std::optional<Transition> baseline_transition_from_initial(int function_state);

bool is_recovered_action_button_state(int function_state);
std::string status_report();

}  // namespace nevergone::meplayer_function_state_contract
