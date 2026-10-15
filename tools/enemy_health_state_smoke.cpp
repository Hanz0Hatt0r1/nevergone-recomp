#include <cassert>
#include <cmath>

#include "../android/app/src/main/cpp/enemy_health_state.h"

using nevergone::enemy_health_state::State;
using nevergone::enemy_health_state::add_health;
using nevergone::enemy_health_state::apply_beaten_damage;
using nevergone::enemy_health_state::health_ratio_percent;
using nevergone::enemy_health_state::initialize;
using nevergone::enemy_health_state::is_dead;
using nevergone::enemy_health_state::set_dead;
using nevergone::enemy_health_state::set_enemy_state;
using nevergone::enemy_health_state::set_force_action_state_no_dead;

namespace {

bool near(float left, float right) {
    return std::fabs(left - right) < 0.0001f;
}

}  // namespace

int main() {
    State state;
    assert(!initialize(0, &state));
    assert(!initialize(-1, &state));
    assert(!initialize(100, nullptr));

    assert(initialize(100, &state));
    assert(near(state.max_health, 100.0f));
    assert(near(state.current_health, 100.0f));
    assert(near(health_ratio_percent(state), 100.0f));
    assert(!is_dead(state));

    apply_beaten_damage(&state, 30.0f);
    assert(near(state.current_health, 70.0f));
    assert(near(health_ratio_percent(state), 70.0f));
    assert(!is_dead(state));

    // The shipped ordinary subtraction is not clamped at zero.
    apply_beaten_damage(&state, 80.0f);
    assert(near(state.current_health, -10.0f));
    assert(near(health_ratio_percent(state), -10.0f));
    assert(is_dead(state));

    // Enemy states 0x0f and 0x2c suppress the ordinary death predicate even
    // when current health is non-positive.
    set_enemy_state(&state, 0x0f);
    assert(!is_dead(state));
    set_enemy_state(&state, 0x2c);
    assert(!is_dead(state));
    set_enemy_state(&state, 0x01);
    assert(is_dead(state));

    // forceCurActionStateNoDead() is a second independent suppression gate.
    set_force_action_state_no_dead(&state, true);
    assert(!is_dead(state));
    set_force_action_state_no_dead(&state, false);
    assert(is_dead(state));

    // addHealth() is likewise unclamped in the original core mutation.
    add_health(&state, 20.0f);
    assert(near(state.current_health, 10.0f));
    assert(!is_dead(state));

    set_dead(&state);
    assert(near(state.current_health, 0.0f));
    assert(is_dead(state));

    return 0;
}
