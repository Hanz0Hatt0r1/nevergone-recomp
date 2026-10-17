#include "player_health_contract.h"

#include <cmath>

namespace nevergone::player_health_contract {
namespace {

std::optional<std::uint32_t> team_health_bar_value(float health) noexcept {
    constexpr double kUint32Limit = 4294967296.0;
    if (!std::isfinite(health) || health < 0.0f ||
        static_cast<double>(health) >= kUint32Limit) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(health);
}

}  // namespace

float set_health(float requested, std::int32_t maximum_health) noexcept {
    const float maximum = static_cast<float>(maximum_health);
    if (requested > maximum) return maximum;
    if (requested < 0.0f) return 0.0f;
    return requested;
}

TeamHealthUpdate set_team_health(
        std::int32_t requested,
        std::int32_t maximum_health) noexcept {
    TeamHealthUpdate update;
    update.current_health = set_health(
        static_cast<float>(requested), maximum_health);
    update.health_bar_value = team_health_bar_value(update.current_health);
    return update;
}

bool is_beaten_state(std::uint32_t state) noexcept {
    return (state >= 0x22u && state <= 0x23u) ||
        (state >= 0x28u && state <= 0x2du) ||
        (state >= 0x3du && state <= 0x3eu) ||
        state == 0x41u;
}

}  // namespace nevergone::player_health_contract
