#include <cassert>

#include "player_function_action_contract.h"

namespace contract = nevergone::player_function_action_contract;

int main() {
    const auto a = contract::base_action_request(7u);
    assert(a.has_value());
    assert(a->controls_function_state == 7u);
    assert(a->meplayer_state == 0x02u);
    assert(!a->force);
    assert(!a->has_additional_player_guard);

    // Starting from the project's recovered initial MEPLAYERSTATE 0x1b, the
    // native setFunctionState(8) baseline falls through to state 0x1d. The
    // alternate 0x11/force=true guarded path remains state-dependent.
    const auto b = contract::base_action_request(8u);
    assert(b.has_value());
    assert(b->meplayer_state == 0x1du);
    assert(!b->force);
    assert(b->has_additional_player_guard);

    const auto c = contract::base_action_request(9u);
    assert(c.has_value());
    assert(c->meplayer_state == 0x20u);
    assert(!c->force);
    assert(!c->has_additional_player_guard);

    // Combo/function states are deliberately not guessed by this base mapping.
    assert(!contract::base_action_request(0u).has_value());
    assert(!contract::base_action_request(0x14u).has_value());
    assert(!contract::base_action_request(0x1cu).has_value());
    return 0;
}
