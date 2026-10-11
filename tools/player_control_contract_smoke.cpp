#include "player_control_contract.h"

#include <cassert>
#include <limits>
#include <string>

namespace contract = nevergone::player_control_contract;

int main() {
    using State = contract::PrimaryControlState;
    using Target = contract::TickTarget;

    assert(!contract::horizontal_primary_state(0.05f, 0.0f).has_value());
    assert(!contract::horizontal_primary_state(0.0f, 0.0f).has_value());
    assert(contract::horizontal_primary_state(0.051f, 0.0f) == State::kState13);
    assert(contract::horizontal_primary_state(1.0f, -90.0f) == State::kState13);
    assert(contract::horizontal_primary_state(1.0f, 90.0f) == State::kState13);
    assert(contract::horizontal_primary_state(1.0f, 90.01f) == State::kState12);
    assert(contract::horizontal_primary_state(1.0f, -90.01f) == State::kState12);
    assert(contract::horizontal_primary_state(1.0f, -180.0f) == State::kState12);
    assert(!contract::horizontal_primary_state(1.0f, -180.01f).has_value());
    assert(!contract::horizontal_primary_state(
        std::numeric_limits<float>::quiet_NaN(), 0.0f).has_value());
    assert(!contract::horizontal_primary_state(
        1.0f, std::numeric_limits<float>::infinity()).has_value());

    assert(contract::facing_flip_x(State::kState12) == true);
    assert(contract::facing_flip_x(State::kState13) == false);
    assert(!contract::facing_flip_x(static_cast<State>(0x7f)).has_value());

    const auto dispatches = contract::tick_dispatches(7, 8, 9, 0x13);
    assert(dispatches[0].target == Target::kDirCombo);
    assert(dispatches[0].state == 7);
    assert(dispatches[1].target == Target::kDirection);
    assert(dispatches[1].state == 8);
    assert(dispatches[2].target == Target::kFunction);
    assert(dispatches[2].state == 9);
    assert(dispatches[3].target == Target::kPrimary);
    assert(dispatches[3].state == 0x13);

    assert(std::string(contract::state_name(State::kState12)) == "state-0x12");
    assert(std::string(contract::state_name(State::kState13)) == "state-0x13");
    assert(std::string(contract::target_name(Target::kDirCombo)) == "dir-combo");
    assert(std::string(contract::target_name(Target::kDirection)) == "direction");
    assert(std::string(contract::target_name(Target::kFunction)) == "function");
    assert(std::string(contract::target_name(Target::kPrimary)) == "primary");
    return 0;
}
