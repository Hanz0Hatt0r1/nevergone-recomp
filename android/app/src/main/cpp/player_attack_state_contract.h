#pragma once

#include <cstdint>

namespace nevergone::player_attack_state_contract {

// Exact unsigned predicate from MEPlayer::isAttackState(), which reads
// MEPlayer+0x278. Keep the wrapping uint32_t subtractions: the native Thumb
// code compares those subtractions as unsigned values.
inline bool is_attack_state(std::uint32_t action_state) noexcept {
    const bool excluded_state_range =
            (0xeu < action_state - 0x2u) &&
            (0x1u < action_state - 0x35u) &&
            (0x4u < action_state - 0x12u) &&
            (action_state != 0x32u) &&
            (0x2u < action_state - 0x18u) &&
            (action_state != 0x27u) &&
            (action_state != 0x2eu) &&
            (action_state != 0x38u) &&
            (0x2u < action_state - 0x3au);
    return !excluded_state_range || action_state == 0x3fu;
}

}  // namespace nevergone::player_attack_state_contract
