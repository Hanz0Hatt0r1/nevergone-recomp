#pragma once

#include <cstdint>
#include <optional>

namespace nevergone::player_hl_point_contract {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct MovementBoundsCache {
    bool valid = false;
    Rect bounds;
};

struct PlayerBoundsResult {
    Rect bounds;
    MovementBoundsCache movement_cache;
};

// MeActionsSystem::actionBoundingBox() returns a unit rectangle when the
// primary sprite (+0x118) is absent, otherwise it returns sprBoundingBox().
std::optional<Rect> action_bounds(std::optional<Rect> sprite_bounds) noexcept;

// Mirrors MEPlayer::playerBoundingBox(). The three position inputs correspond
// to the native points at +0x240, +0x248 and +0x268. The cache corresponds to
// +0x3f0/+0x400 and is populated on the first action state 0 or 1 lookup.
std::optional<PlayerBoundsResult> player_bounds(
        std::optional<Rect> sprite_bounds,
        std::uint32_t action_state,
        Point position_240,
        Point offset_248,
        Point position_268,
        MovementBoundsCache movement_cache) noexcept;

// Mirrors MEPlayer::updateHLValue(): use the AABB bottom-center as the
// playerHLPoint, with the recovered state gates that preserve its Y component.
std::optional<Point> update_hl_point(
        Rect player_bounds,
        std::uint32_t action_state,
        std::uint32_t selector_144,
        Point previous_hl_point) noexcept;

}  // namespace nevergone::player_hl_point_contract
