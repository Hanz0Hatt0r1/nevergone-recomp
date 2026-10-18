#include "player_function_action_contract.h"

namespace nevergone::player_function_action_contract {

std::optional<ActionRequest> base_action_request(
        std::uint32_t controls_function_state,
        std::optional<std::uint32_t> current_action_state,
        std::uint32_t player_index) {
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
        case 0x15u:
            // Native setFunctionState(0x15) uses this request from initial
            // state 0x1b. Career 1 has a recovered 0x0f -> 0x1b completion
            // path; other careers use additional player data not modeled here.
            if (player_index == 1u && current_action_state == 0x1bu) {
                return ActionRequest{0x15u, 0x0fu, false, false};
            }
            return std::nullopt;
        case 0x14u:
            // The A + direction-combo-10 route calls setActionState(0x0c,true)
            // from the initial state. Career 1 has a recovered 0x0c -> 0x0d ->
            // 0x0e -> 0x1b completion chain.
            if (player_index == 1u && current_action_state == 0x1bu) {
                return ActionRequest{0x14u, 0x0cu, true, false};
            }
            return std::nullopt;
        default:
            return std::nullopt;
    }
}

}  // namespace nevergone::player_function_action_contract
