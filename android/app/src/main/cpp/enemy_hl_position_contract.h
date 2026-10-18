#pragma once

#include <cmath>
#include <cstdint>
#include <optional>

namespace nevergone::enemy_hl_position_contract {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

// Mirrors the origin-point branch of EnemyObject::enemyHLPos(). The caller
// supplies the bounding point after enemyBoundingBox() and GetFrameYOffset().
inline std::optional<Point> origin_hit_point(
        std::uint32_t enemy_state,
        std::int32_t field_248,
        Point bounding_point_17c,
        Point object_point_280,
        Point previous_point_184) noexcept {
    Point point = bounding_point_17c;
    if (enemy_state == 0x0fu || enemy_state == 0x11u) {
        point = previous_point_184;
    } else if (field_248 != 1 &&
            (enemy_state == 0x1cu || enemy_state == 0x2eu || enemy_state == 0x2fu)) {
        point = object_point_280;
    }

    if (!std::isfinite(point.x) || !std::isfinite(point.y)) return std::nullopt;
    return point;
}

}  // namespace nevergone::enemy_hl_position_contract
