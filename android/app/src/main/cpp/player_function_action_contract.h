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

// Maps the three base Controls::tick() function states into the exact
// MEPlayer::setActionState(state, force) request reconstructed from the shipped
// ARMv7 MEPlayer::setFunctionState implementation. This does not bypass the
// original current-player-state guards.
std::optional<ActionRequest> base_action_request(std::uint32_t controls_function_state);

}  // namespace nevergone::player_function_action_contract
