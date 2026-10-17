#include "enemy_hitpoint_contract.h"

namespace nevergone::enemy_hitpoint_contract {

std::optional<HitPoint> resolve(
        const enemy_actions_wbg_document::Document& document,
        std::int32_t current_frame,
        std::int32_t frame_ordinal,
        std::optional<Point> sprite_bounds_origin,
        float enemy_offset_118,
        float enemy_offset_284) noexcept {
    if (!sprite_bounds_origin.has_value()) return std::nullopt;
    const auto record = action_hitpoint_record_contract::select_record(
        document, current_frame, frame_ordinal);
    if (!record.has_value()) return std::nullopt;

    Point point = *sprite_bounds_origin;
    point.x += enemy_offset_118;
    point.y += enemy_offset_284;
    return HitPoint{*record, point};
}

}  // namespace nevergone::enemy_hitpoint_contract
