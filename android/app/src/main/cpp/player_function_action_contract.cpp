#include "player_function_action_contract.h"

namespace nevergone::player_function_action_contract {

std::optional<ActionRequest> base_action_request(
        std::uint32_t controls_function_state,
        std::optional<std::uint32_t> current_action_state) {
    switch (controls_function_state) {
        case 7u:
            if (!current_action_state.has_value()) return std::nullopt;
            switch (*current_action_state) {
                case 0x00u:
                    // setActionState(2,false) from walk selects state 0x10.
                    return ActionRequest{7u, 0x10u, false, false};
                case 0x01u:
                    // setActionState(2,false) from run selects state 0x14.
                    return ActionRequest{7u, 0x14u, false, false};
                case 0x1bu:
                case 0x1cu:
                    // The native path normalizes its temporary 0x27 state to 2.
                    return ActionRequest{7u, 0x02u, false, false};
                default:
                    return std::nullopt;
            }
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
