#include "player_function_action_contract.h"

namespace nevergone::player_function_action_contract {

std::optional<ActionRequest> base_action_request(std::uint32_t controls_function_state) {
    switch (controls_function_state) {
        case 7u:
            // MEPlayer::setFunctionState: base A path -> setActionState(2,false).
            return ActionRequest{7u, 0x02u, false, false};
        case 8u:
            // Base B path is conditioned by current MEPlayer fields at +0x1f0,
            // +0x1f8 and +0x278 before setActionState(0x11,true).
            return ActionRequest{8u, 0x11u, true, true};
        case 9u:
            // Base C path -> setActionState(0x20,false).
            return ActionRequest{9u, 0x20u, false, false};
        default:
            return std::nullopt;
    }
}

}  // namespace nevergone::player_function_action_contract
