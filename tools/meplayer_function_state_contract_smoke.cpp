#include <cassert>
#include <string>

#include "meplayer_function_state_contract.h"

namespace contract = nevergone::meplayer_function_state_contract;

int main() {
    assert(contract::is_recovered_action_button_state(7));
    assert(contract::is_recovered_action_button_state(8));
    assert(contract::is_recovered_action_button_state(9));
    assert(!contract::is_recovered_action_button_state(0));

    auto transition = contract::unconditional_transition(7);
    assert(transition.has_value());
    assert(transition->action_state == 0x02u);
    assert(!transition->blend_or_restart);

    transition = contract::unconditional_transition(9);
    assert(transition.has_value());
    assert(transition->action_state == 0x20u);
    assert(!transition->blend_or_restart);

    assert(!contract::unconditional_transition(8).has_value());

    transition = contract::baseline_transition_from_initial(8);
    assert(transition.has_value());
    assert(transition->action_state == 0x1du);
    assert(!transition->blend_or_restart);

    const std::string status = contract::status_report();
    assert(status.find("action03") != std::string::npos);
    assert(status.find("action30") != std::string::npos);
    assert(status.find("action33") != std::string::npos);
    return 0;
}
