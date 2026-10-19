#pragma once

#include <cstdint>
#include <optional>

namespace nevergone::player_health_contract {

struct TeamHealthUpdate {
    float current_health = 0.0f;
    std::optional<std::uint32_t> health_bar_value;
};

// Mirrors MEPlayer::setHealth(): clamp the requested value against the signed
// maximum at +0x11c, then clamp negative health to zero.
float set_health(float requested, std::int32_t maximum_health) noexcept;

// Mirrors MEPlayer::setHealth_T(): convert the signed request to float, apply
// the same clamp, then expose the unsigned, truncating value sent to the team
// health bar when that conversion is in its defined finite range.
TeamHealthUpdate set_team_health(
    std::int32_t requested,
    std::int32_t maximum_health) noexcept;

// MEPlayer::isBeatenState() accepts these exact state ranges from +0x278.
bool is_beaten_state(std::uint32_t state) noexcept;

}  // namespace nevergone::player_health_contract
