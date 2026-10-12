#include <cassert>
#include <vector>

#include "game_scene_action_lifecycle_contract.h"

namespace lifecycle = nevergone::game_scene_action_lifecycle_contract;

int main() {
    const auto first = lifecycle::trigger_once(false, 2u);
    assert(first.accepted);
    assert(first.triggered_once);
    assert(first.commands.size() == 6u);
    assert(first.commands[0].kind == lifecycle::CommandKind::kPlay);
    assert(first.commands[0].action_index == 0u);
    assert(first.commands[1].kind == lifecycle::CommandKind::kApplyReturnedPosition);
    assert(first.commands[2].kind == lifecycle::CommandKind::kAttachAtObjectZ);
    assert(first.commands[3].kind == lifecycle::CommandKind::kPlay);
    assert(first.commands[3].action_index == 1u);
    assert(first.commands[4].kind == lifecycle::CommandKind::kApplyReturnedPosition);
    assert(first.commands[5].kind == lifecycle::CommandKind::kAttachAtObjectZ);

    const auto repeated = lifecycle::trigger_once(true, 2u);
    assert(!repeated.accepted);
    assert(repeated.triggered_once);
    assert(repeated.commands.empty());

    // Generic tick: active entries step, done entries stop+detach.
    const auto generic = lifecycle::tick({false, true}, 0u, 0u);
    assert(generic.special_event_state == 0u);
    assert(generic.commands.size() == 3u);
    assert(generic.commands[0].kind == lifecycle::CommandKind::kStep);
    assert(generic.commands[0].action_index == 0u);
    assert(generic.commands[1].kind == lifecycle::CommandKind::kStop);
    assert(generic.commands[1].action_index == 1u);
    assert(generic.commands[2].kind == lifecycle::CommandKind::kDetach);
    assert(generic.commands[2].action_index == 1u);

    // The 0x12e branch is explicit and must not leak into generic klhuo flow.
    const auto special_start = lifecycle::trigger_special_event(
            lifecycle::kSpecialEventId,
            lifecycle::kSpecialEventId,
            0u,
            false,
            2u);
    assert(special_start.accepted);
    assert(special_start.special_event_state == 1u);
    assert(special_start.commands.size() == 2u);
    assert(special_start.commands[0].kind == lifecycle::CommandKind::kPlay);
    assert(special_start.commands[0].action_index == 0u);
    assert(special_start.commands[1].kind == lifecycle::CommandKind::kAttachAtObjectZ);

    const auto special_tick = lifecycle::tick(
            {true, false}, lifecycle::kSpecialEventId, 1u);
    assert(special_tick.special_event_state == 2u);
    assert(special_tick.commands.size() == 5u);
    assert(special_tick.commands[0].kind == lifecycle::CommandKind::kStop);
    assert(special_tick.commands[0].action_index == 0u);
    assert(special_tick.commands[1].kind == lifecycle::CommandKind::kDetach);
    assert(special_tick.commands[2].kind == lifecycle::CommandKind::kPlay);
    assert(special_tick.commands[2].action_index == 1u);
    assert(special_tick.commands[3].kind == lifecycle::CommandKind::kAttachAtObjectZ);
    assert(special_tick.commands[4].kind == lifecycle::CommandKind::kStep);
    assert(special_tick.commands[4].action_index == 1u);

    const auto rejected_event = lifecycle::trigger_special_event(
            0x12du, 0x12du, 0u, false, 2u);
    assert(!rejected_event.accepted);
    assert(rejected_event.commands.empty());

    return 0;
}
