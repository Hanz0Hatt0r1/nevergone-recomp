#include "controls_function_state_contract.h"

#include <cmath>

namespace nevergone::controls_function_state_contract {
namespace {

bool primary_timer_combo(std::uint32_t state) {
    return state == 1u || state == 2u || state == 10u || state == 11u;
}

bool secondary_timer_combo(std::uint32_t state) {
    return state >= 14u && state <= 19u;
}

}  // namespace

std::optional<Output> step(const Input& input) {
    if (!std::isfinite(input.primary_combo_timer_seconds) ||
        !std::isfinite(input.secondary_combo_timer_seconds) ||
        !std::isfinite(input.delta_seconds) ||
        input.primary_combo_timer_seconds < 0.0f ||
        input.secondary_combo_timer_seconds < 0.0f ||
        input.delta_seconds < 0.0f) {
        return std::nullopt;
    }

    Output out{
        input.previous_function_state,
        input.direction_combo_state,
        input.primary_combo_timer_seconds,
        input.secondary_combo_timer_seconds,
    };

    if (!input.player_available) {
        out.function_state = 0u;
        return out;
    }

    // Controls::tick expires +0x154 strictly above 0.5 s and clears the four
    // short primary direction-combo states at the same boundary.
    if (out.primary_combo_timer_seconds > 0.5f) {
        out.primary_combo_timer_seconds = 0.0f;
        if (primary_timer_combo(out.direction_combo_state)) {
            out.direction_combo_state = 0u;
        }
    }

    if (input.button_a_pressed) {
        if (input.button_c_pressed) {
            out.function_state = 0x1cu;
        } else if (out.function_state == 0u) {
            out.function_state = 7u;

            if (out.primary_combo_timer_seconds < 0.4f &&
                out.direction_combo_state == 10u) {
                out.function_state = 0x14u;
            }

            if (out.primary_combo_timer_seconds < 0.3f) {
                if (out.direction_combo_state == 1u) {
                    out.function_state = 0x16u;
                } else if (out.direction_combo_state == 2u) {
                    out.function_state = 0x15u;
                    out.direction_combo_state = 0u;
                }
            }

            if (out.secondary_combo_timer_seconds < 0.3f) {
                if (out.direction_combo_state == 14u) {
                    out.function_state = 0x17u;
                } else if (out.direction_combo_state == 16u) {
                    out.function_state = 0x18u;
                }
            }

            // The shipped A first-activation path clears both the current
            // direction-combo state and +0x154 after applying the rewrites.
            out.direction_combo_state = 0u;
            out.primary_combo_timer_seconds = 0.0f;
        }
    } else if (input.button_b_pressed) {
        out.function_state = 8u;
        if (out.secondary_combo_timer_seconds < 0.3f &&
            out.direction_combo_state == 10u) {
            out.function_state = 0x1bu;
            out.direction_combo_state = 0u;
        }
    } else if (input.button_c_pressed) {
        out.function_state = 9u;
    }

    if (primary_timer_combo(out.direction_combo_state)) {
        out.primary_combo_timer_seconds += input.delta_seconds;
    } else if (secondary_timer_combo(out.direction_combo_state)) {
        out.secondary_combo_timer_seconds += input.delta_seconds;
    }

    if (!input.button_a_pressed && !input.button_b_pressed && !input.button_c_pressed) {
        out.function_state = 0u;
    }

    return out;
}

}  // namespace nevergone::controls_function_state_contract
