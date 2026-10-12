#pragma once

#include <cstdint>
#include <optional>

namespace nevergone::player_function_action_contract {

struct ActionRequest {
    std::uint32_t controls_function_state = 0;
    std::uint32_t meplayer_state = 0;
    bool force = false;
    bool has_additional_player_guard = false;
};

// Maps the three base Controls::tick() A/B/C function states into the recovered
// MEPlayer::setActionState(state, force) request for the project's current
// initial-player baseline. A and C are direct branches. B is stateful: from the
// recovered initial action 0x1b it requests 0x1d,false, while another guarded
// native branch can request 0x11,true. has_additional_player_guard marks that
// distinction so callers do not treat B as globally unconditional.
std::optional<ActionRequest> base_action_request(std::uint32_t controls_function_state);

}  // namespace nevergone::player_function_action_contract
