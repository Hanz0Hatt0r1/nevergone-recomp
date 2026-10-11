#include "game_scene_player_frame_request.h"

#include <cmath>
#include <cstddef>
#include <utility>

namespace nevergone::game_scene_player_frame_request {
namespace {

constexpr std::size_t kMaxFrameKeyLength = 512u;

bool valid_position(const game_scene_player_bootstrap::InitialPoint& position) {
    return std::isfinite(position.x) && std::isfinite(position.y);
}

bool valid_frame_key(const std::string& value, std::int32_t serialized_length) {
    if (value.empty() || value.size() > kMaxFrameKeyLength || serialized_length <= 0) {
        return false;
    }
    if (static_cast<std::size_t>(serialized_length) != value.size()) return false;
    if (value == "." || value == "..") return false;
    for (const char ch : value) {
        if (ch == '\0' || ch == '/' || ch == '\\') return false;
    }
    return true;
}

}  // namespace

Request build(
        const enemy_actions_wbg_document::Document& document,
        enemy_actions_runtime_state::State state,
        game_scene_player_bootstrap::InitialPoint position) {
    Request result;
    result.position = position;
    result.lookup = enemy_actions_runtime_state::apply_update_data_frame_lookup(document, state);

    if (!result.lookup.entry.readiness_gate_open) {
        result.status = Status::kReadinessGateClosed;
        return result;
    }
    if (!result.lookup.entry.has_primary_frames) {
        result.status = Status::kNoPrimaryFrames;
        return result;
    }
    if (!result.lookup.entry.current_frame_in_range || !result.lookup.should_lookup_sprite_frame) {
        result.status = Status::kFrameOutOfRange;
        return result;
    }
    if (!valid_position(position)) {
        result.status = Status::kInvalidPosition;
        return result;
    }
    if (!valid_frame_key(result.lookup.frame_name_68, result.lookup.frame_name_length_6c)) {
        result.status = Status::kInvalidFrameKey;
        return result;
    }

    game_scene_type0_resource::Selection resource;
    resource.kind = game_scene_type0_resource::Kind::kSpriteFrameByName;
    resource.resource_name = result.lookup.frame_name_68;
    result.resource = std::move(resource);
    result.status = Status::kReady;
    return result;
}

const char* status_name(Status status) {
    switch (status) {
        case Status::kReadinessGateClosed: return "readiness-gate-closed";
        case Status::kNoPrimaryFrames: return "no-primary-frames";
        case Status::kFrameOutOfRange: return "frame-out-of-range";
        case Status::kInvalidPosition: return "invalid-position";
        case Status::kInvalidFrameKey: return "invalid-frame-key";
        case Status::kReady: return "ready";
    }
    return "unknown";
}

}  // namespace nevergone::game_scene_player_frame_request
