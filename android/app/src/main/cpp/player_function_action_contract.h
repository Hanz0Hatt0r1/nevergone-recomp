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

// Maps base Controls::tick() A/B/C function states into recovered
// MEPlayer::setActionState(state, force) requests. A depends on the current
// locomotion state: walk 0 -> 0x10, run 1 -> 0x14, and initial 0x1b (or 0x1c)
// -> 0x02. B uses the project's initial-player baseline 0x1b and requests
// 0x1d,false; another guarded native branch can request 0x11,true.
std::optional<ActionRequest> base_action_request(
    std::uint32_t controls_function_state,
    std::optional<std::uint32_t> current_action_state =
        std::optional<std::uint32_t>{0x1bu});

}  // namespace nevergone::player_function_action_contract
