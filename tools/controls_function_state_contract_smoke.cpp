#include <cassert>
#include <cmath>

#include "controls_function_state_contract.h"

namespace contract = nevergone::controls_function_state_contract;

namespace {

contract::Output run(contract::Input input) {
    const auto output = contract::step(input);
    assert(output.has_value());
    return *output;
}

bool near(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

}  // namespace

int main() {
    contract::Input input{};

    input.button_a_pressed = true;
    auto out = run(input);
    assert(out.function_state == 7u);

    input = {};
    input.button_b_pressed = true;
    out = run(input);
    assert(out.function_state == 8u);

    input = {};
    input.button_c_pressed = true;
    out = run(input);
    assert(out.function_state == 9u);

    // A owns priority and A+C is an explicit shipped combined function state.
    input = {};
    input.button_a_pressed = true;
    input.button_b_pressed = true;
    out = run(input);
    assert(out.function_state == 7u);
    input.button_c_pressed = true;
    out = run(input);
    assert(out.function_state == 0x1cu);

    // A only recomputes its base/combination state when +0x118 was zero.
    input = {};
    input.button_a_pressed = true;
    input.previous_function_state = 0x14u;
    out = run(input);
    assert(out.function_state == 0x14u);

    input = {};
    input.button_a_pressed = true;
    input.direction_combo_state = 10u;
    input.primary_combo_timer_seconds = 0.39f;
    out = run(input);
    assert(out.function_state == 0x14u);
    assert(out.direction_combo_state == 0u);
    assert(near(out.primary_combo_timer_seconds, 0.0f));

    input = {};
    input.button_a_pressed = true;
    input.direction_combo_state = 1u;
    input.primary_combo_timer_seconds = 0.29f;
    out = run(input);
    assert(out.function_state == 0x16u);

    input = {};
    input.button_a_pressed = true;
    input.direction_combo_state = 2u;
    input.primary_combo_timer_seconds = 0.29f;
    out = run(input);
    assert(out.function_state == 0x15u);

    input = {};
    input.button_a_pressed = true;
    input.direction_combo_state = 14u;
    input.secondary_combo_timer_seconds = 0.29f;
    out = run(input);
    assert(out.function_state == 0x17u);

    input = {};
    input.button_a_pressed = true;
    input.direction_combo_state = 16u;
    input.secondary_combo_timer_seconds = 0.29f;
    out = run(input);
    assert(out.function_state == 0x18u);

    input = {};
    input.button_b_pressed = true;
    input.direction_combo_state = 10u;
    input.secondary_combo_timer_seconds = 0.29f;
    out = run(input);
    assert(out.function_state == 0x1bu);
    assert(out.direction_combo_state == 0u);

    // Primary combos/timer expire strictly above 0.5 s, then B falls back to 8.
    input = {};
    input.button_b_pressed = true;
    input.direction_combo_state = 10u;
    input.primary_combo_timer_seconds = 0.51f;
    input.secondary_combo_timer_seconds = 0.1f;
    out = run(input);
    assert(out.function_state == 8u);
    assert(out.direction_combo_state == 0u);
    assert(near(out.primary_combo_timer_seconds, 0.0f));

    // Timer advancement happens after button synthesis and only for the active
    // recovered direction-combo timer family.
    input = {};
    input.direction_combo_state = 11u;
    input.delta_seconds = 0.125f;
    out = run(input);
    assert(out.function_state == 0u);
    assert(near(out.primary_combo_timer_seconds, 0.125f));

    input = {};
    input.direction_combo_state = 18u;
    input.delta_seconds = 0.2f;
    out = run(input);
    assert(near(out.secondary_combo_timer_seconds, 0.2f));

    input = {};
    input.player_available = false;
    input.previous_function_state = 9u;
    input.direction_combo_state = 11u;
    input.primary_combo_timer_seconds = 0.2f;
    input.delta_seconds = 0.1f;
    out = run(input);
    assert(out.function_state == 0u);
    assert(out.direction_combo_state == 11u);
    assert(near(out.primary_combo_timer_seconds, 0.2f));

    input = {};
    input.delta_seconds = -0.1f;
    assert(!contract::step(input).has_value());
    return 0;
}
