#pragma once

#include <cstdint>

namespace nevergone::player_collision_state_contract {

// Mirrors MEPlayer::isCollisionState(). The first argument is MEPlayer+0x144,
// the selected player index; collision_disabled is the byte at +0x3e8.
bool is_collision_state(
        std::uint32_t player_index,
        std::uint32_t action_state,
        bool collision_disabled) noexcept;

}  // namespace nevergone::player_collision_state_contract
