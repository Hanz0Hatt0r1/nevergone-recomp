#pragma once

#include <cstdint>
#include <optional>

namespace nevergone::controls_function_state_contract {

struct Input {
    bool player_available = true;
    bool button_a_pressed = false;
    bool button_b_pressed = false;
    bool button_c_pressed = false;
    std::uint32_t previous_function_state = 0;
    std::uint32_t direction_combo_state = 0;
    float primary_combo_timer_seconds = 0.0f;   // Controls +0x154
    float secondary_combo_timer_seconds = 0.0f; // Controls +0x158
    float delta_seconds = 0.0f;
};

struct Output {
    std::uint32_t function_state = 0;
    std::uint32_t direction_combo_state = 0;
    float primary_combo_timer_seconds = 0.0f;
    float secondary_combo_timer_seconds = 0.0f;
};

// Reproduces the action-button/function-state subset of shipped Controls::tick.
// It covers the exact A/B/C priority, A+C overlap, direction-combo rewrites,
// timer expiry/reset and timer advancement that feed Controls +0x118.
// Returns nullopt for non-finite/negative timer inputs rather than inventing
// behavior outside the recovered domain.
std::optional<Output> step(const Input& input);

}  // namespace nevergone::controls_function_state_contract
