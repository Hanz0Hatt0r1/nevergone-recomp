#include "player_hl_point_contract.h"

#include <cmath>

namespace nevergone::player_hl_point_contract {
namespace {

bool finite(const Rect& rect) {
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
            std::isfinite(rect.width) && std::isfinite(rect.height);
}

bool finite(Point point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

bool in_range(std::uint32_t value, std::uint32_t first, std::uint32_t last) {
    return value >= first && value <= last;
}

bool preserves_y(std::uint32_t action_state, std::uint32_t selector_144) {
    if (selector_144 == 1u) {
        return in_range(action_state, 0x19u, 0x1au) ||
                in_range(action_state, 0x35u, 0x36u) ||
                in_range(action_state, 0x04u, 0x0au) ||
                action_state == 0x0fu ||
                in_range(action_state, 0x20u, 0x21u) ||
                action_state == 0x31u || action_state == 0x17u ||
                in_range(action_state, 0x11u, 0x13u);
    }

    return in_range(action_state, 0x19u, 0x1au) ||
            in_range(action_state, 0x35u, 0x36u) ||
            action_state == 0x06u || action_state == 0x0fu ||
            in_range(action_state, 0x20u, 0x21u) ||
            action_state == 0x31u || action_state == 0x38u ||
            in_range(action_state, 0x12u, 0x13u) ||
            action_state == 0x17u || in_range(action_state, 0x0bu, 0x0cu);
}

}  // namespace

std::optional<Rect> action_bounds(std::optional<Rect> sprite_bounds) noexcept {
    if (!sprite_bounds.has_value()) return Rect{0.0f, 0.0f, 1.0f, 1.0f};
    if (!finite(*sprite_bounds)) return std::nullopt;
    return sprite_bounds;
}

std::optional<PlayerBoundsResult> player_bounds(
        std::optional<Rect> sprite_bounds,
        std::uint32_t action_state,
        Point position_240,
        Point offset_248,
        Point position_268,
        MovementBoundsCache movement_cache) noexcept {
    const auto action = action_bounds(sprite_bounds);
    if (!action.has_value() || !finite(position_240) || !finite(offset_248) ||
            !finite(position_268)) {
        return std::nullopt;
    }

    Rect result = *action;
    if (action_state == 0x11u || in_range(action_state, 0x2fu, 0x30u)) {
        result.x = position_240.x + offset_248.x - action->width * 0.5f;
        result.y = position_240.y + offset_248.y + action->y;
    } else if (action_state <= 1u) {
        if (!movement_cache.valid) {
            movement_cache.bounds = *action;
            movement_cache.valid = true;
        }
        if (!finite(movement_cache.bounds)) return std::nullopt;
        result.x = position_240.x + offset_248.x - action->width * 0.5f;
        result.y = position_240.y + offset_248.y + movement_cache.bounds.y;
    } else {
        result.x = position_268.x + action->x;
        result.y = position_268.y + action->y;
    }

    if (!finite(result)) return std::nullopt;
    return PlayerBoundsResult{result, movement_cache};
}

std::optional<Point> update_hl_point(
        Rect bounds,
        std::uint32_t action_state,
        std::uint32_t selector_144,
        Point previous_hl_point) noexcept {
    if (!finite(bounds) || !finite(previous_hl_point)) return std::nullopt;

    Point result{bounds.x + bounds.width * 0.5f, bounds.y};
    if (preserves_y(action_state, selector_144)) {
        result.y = previous_hl_point.y;
    }
    if (!finite(result)) return std::nullopt;
    return result;
}

}  // namespace nevergone::player_hl_point_contract
