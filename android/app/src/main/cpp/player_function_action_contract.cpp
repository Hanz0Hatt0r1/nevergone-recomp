#include "player_function_action_contract.h"

namespace nevergone::player_function_action_contract {

std::optional<ActionRequest> base_action_request(std::uint32_t controls_function_state) {
    switch (controls_function_state) {
        case 7u:
            // MEPlayer::setFunctionState: base A path -> setActionState(2,false).
            return ActionRequest{7u, 0x02u, false, false};
        case 8u:
            // From the reconstructed initial player action 0x1b, with no prior
            // guarded-action history at +0x1f0/+0x1f8, the shipped B path falls
            // through to setActionState(0x1d,false). A separate guarded branch
            // can select 0x11,true for other live-state combinations, so callers
            // must still treat B as stateful rather than globally unconditional.
            return ActionRequest{8u, 0x1du, false, true};
        case 9u:
            // Base C path -> setActionState(0x20,false).
            return ActionRequest{9u, 0x20u, false, false};
        default:
            return std::nullopt;
    }
}

}  // namespace nevergone::player_function_action_contract
