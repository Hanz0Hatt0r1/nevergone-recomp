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

// Maps recovered Controls function states into MEPlayer action requests. The
// directional A and super-attack routes are supported only for Career 1 from
// initial state 0x1b; the latter also requires the tutorial's full-Rage event.
std::optional<ActionRequest> base_action_request(
    std::uint32_t controls_function_state,
    std::optional<std::uint32_t> current_action_state =
        std::optional<std::uint32_t>{0x1bu},
    std::uint32_t player_index = 0u,
    bool super_ready = false);

}  // namespace nevergone::player_function_action_contract
