#include "enemy_attack_value_contract.h"

#include <cmath>

namespace nevergone::enemy_attack_value_contract {

std::optional<std::uint32_t> try_initialize_value_2bc(float game_levels_value_20) noexcept {
    if (!std::isfinite(game_levels_value_20) || game_levels_value_20 < 0.0f ||
        static_cast<double>(game_levels_value_20) >= 4294967296.0) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(game_levels_value_20);
}

float resolve(std::uint32_t value_2bc, float current_frame_damage_factor) noexcept {
    return static_cast<float>(value_2bc) * current_frame_damage_factor;
}

}  // namespace nevergone::enemy_attack_value_contract
