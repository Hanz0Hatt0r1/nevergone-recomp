#pragma once

#include <cstdint>
#include <optional>

#include "action_hitpoint_record_contract.h"
#include "enemy_actions_wbg_document.h"

namespace nevergone::enemy_hitpoint_contract {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct HitPoint {
    action_hitpoint_record_contract::RecordRef record;
    Point point;
};

// EnemyObject::beatHitPoint() selects the frame/ordinal record, consumes the
// transformed CCSprite::sprBoundingBox() center, and adds offsets +0x118/+0x284.
// The optional sprite center represents the native parallel +0x150 sprite
// lookup/bounds call; no result is returned if that bounds source is absent.
std::optional<HitPoint> resolve(
    const enemy_actions_wbg_document::Document& document,
    std::int32_t current_frame,
    std::int32_t frame_ordinal,
    std::optional<Point> sprite_bounds_center,
    float enemy_offset_118,
    float enemy_offset_284) noexcept;

}  // namespace nevergone::enemy_hitpoint_contract
