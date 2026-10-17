#pragma once

#include <cstdint>
#include <optional>

#include "action_hitpoint_record_contract.h"
#include "enemy_actions_wbg_document.h"

namespace nevergone::player_hitpoint_contract {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct HitPoint {
    action_hitpoint_record_contract::RecordRef record;
    Point point;
};

// MEPlayer::beatHitPoint() requires a current-frame record and a successful
// MeActionsSystem::curFrameHitPoint() bounds center, then adds player offsets
// +0x268/+0x26c before writing the output CCPoint.
std::optional<HitPoint> resolve(
    const enemy_actions_wbg_document::Document& document,
    std::int32_t current_frame,
    std::int32_t frame_ordinal,
    std::optional<Point> sprite_bounds_center,
    float player_offset_268,
    float player_offset_26c) noexcept;

}  // namespace nevergone::player_hitpoint_contract
