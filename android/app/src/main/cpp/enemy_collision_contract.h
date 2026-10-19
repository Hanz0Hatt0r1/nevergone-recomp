#pragma once

#include <cmath>
#include <cstdint>

#include "enemy_hit_gate_contract.h"

namespace nevergone::enemy_collision_contract {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct Rect {
    float origin_x = 0.0f;
    float origin_y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct BoundingBoxInput {
    bool has_action_system_10c = false;
    std::uint8_t action_bounds_enabled_26c = 0;
    bool has_action_sprite_10c = false;
    Rect action_sprite_bounds;
    std::uint32_t enemy_state_274 = 0;
    std::uint8_t action_move_state_2b6 = 0;
    float offset_280 = 0.0f;
    float offset_284 = 0.0f;
    float offset_118 = 0.0f;
    float offset_11c = 0.0f;
};

struct BeatenState {
    bool has_action_system = false;
    std::uint8_t action_gate_2b5 = 0;
    std::int32_t field_248 = 0;
    std::int32_t field_2a0 = 0;
    std::int32_t enemy_state_274 = 0;
};

struct BeatenResult {
    Point stored_point_314;
    bool hit = false;
};

// EnemyActionsSystem::actionBoundingBox() uses a 1x1 rect at the origin unless
// both the bounds flag and sprite pointer are present.
inline Rect action_bounding_box(
        bool bounds_enabled_26c,
        bool has_action_sprite_10c,
        Rect sprite_bounds) noexcept {
    return bounds_enabled_26c && has_action_sprite_10c
        ? sprite_bounds
        : Rect{0.0f, 0.0f, 1.0f, 1.0f};
}

// EnemyObject::enemyBoundingBox() translates the action rect by one of two
// recovered enemy offsets, selected through isEnemyMoveState().
inline Rect enemy_bounding_box(const BoundingBoxInput& input) noexcept {
    if (!input.has_action_system_10c) {
        return {0.0f, 0.0f, 1.0f, 1.0f};
    }
    Rect bounds = action_bounding_box(
        input.action_bounds_enabled_26c != 0,
        input.has_action_sprite_10c,
        input.action_sprite_bounds);
    const bool moving = enemy_hit_gate_contract::is_enemy_move_state(
        input.enemy_state_274,
        true,
        input.action_move_state_2b6);
    bounds.origin_x += moving ? input.offset_280 : input.offset_118;
    bounds.origin_y += moving ? input.offset_284 : input.offset_11c;
    return bounds;
}

// cocos2d::CCRect::containsPoint(): origin and origin + size are inclusive.
inline bool contains_point(Rect rect, Point point) noexcept {
    const float max_x = rect.origin_x + rect.width;
    const float max_y = rect.origin_y + rect.height;
    if (std::isunordered(point.x, rect.origin_x) ||
            std::isunordered(point.x, max_x) ||
            std::isunordered(point.y, rect.origin_y) ||
            std::isunordered(point.y, max_y)) {
        return false;
    }
    return !(point.x < rect.origin_x || max_x < point.x ||
        point.y < rect.origin_y || max_y < point.y);
}

// EnemyObject::isBeaten() stores the candidate in +0x314 before its guards.
// The caller supplies the transformed rect from enemyBoundingBox().
inline BeatenResult is_beaten(
        const BeatenState& state, Point candidate, Rect transformed_bounds) noexcept {
    BeatenResult result{candidate, false};
    if (!state.has_action_system || state.action_gate_2b5 != 0) return result;
    if (state.field_248 != 0 && state.field_2a0 == 0x1a &&
            state.enemy_state_274 == 0x14) {
        return result;
    }
    result.hit = contains_point(transformed_bounds, candidate);
    return result;
}

}  // namespace nevergone::enemy_collision_contract
