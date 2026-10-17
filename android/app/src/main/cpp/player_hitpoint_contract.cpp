#include "player_hitpoint_contract.h"

namespace nevergone::player_hitpoint_contract {

std::optional<HitPoint> resolve(
        const enemy_actions_wbg_document::Document& document,
        std::int32_t current_frame,
        std::int32_t frame_ordinal,
        std::optional<Point> sprite_bounds_center,
        float player_offset_268,
        float player_offset_26c) noexcept {
    if (!sprite_bounds_center.has_value()) return std::nullopt;
    const auto record = action_hitpoint_record_contract::select_record(
        document, current_frame, frame_ordinal);
    if (!record.has_value()) return std::nullopt;

    Point point = *sprite_bounds_center;
    point.x += player_offset_268;
    point.y += player_offset_26c;
    return HitPoint{*record, point};
}

}  // namespace nevergone::player_hitpoint_contract
