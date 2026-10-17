#include "player_collision_state_contract.h"

namespace nevergone::player_collision_state_contract {
namespace {

bool in_range(std::uint32_t value, std::uint32_t first, std::uint32_t last) noexcept {
    return value >= first && value <= last;
}

bool excluded_for_player_two(std::uint32_t state) noexcept {
    return state == 0x11u || in_range(state, 0x2fu, 0x31u) ||
            in_range(state, 0x17u, 0x1au) || in_range(state, 0x20u, 0x21u) ||
            in_range(state, 0x0du, 0x0fu) || in_range(state, 0x35u, 0x36u) ||
            state == 0x38u || state == 0x2du || state == 0x3cu;
}

bool excluded_for_player_one(std::uint32_t state) noexcept {
    return state == 0x11u || in_range(state, 0x2fu, 0x31u) || state == 0x2du ||
            in_range(state, 0x17u, 0x1au) || in_range(state, 0x20u, 0x21u) ||
            state == 0x0fu || in_range(state, 0x0cu, 0x0du) || state == 0x3cu;
}

}  // namespace

bool is_collision_state(
        std::uint32_t player_index,
        std::uint32_t action_state,
        bool collision_disabled) noexcept {
    if (collision_disabled) return false;
    if (player_index == 2u) return !excluded_for_player_two(action_state);
    if (player_index == 1u) return !excluded_for_player_one(action_state);
    return false;
}

}  // namespace nevergone::player_collision_state_contract
