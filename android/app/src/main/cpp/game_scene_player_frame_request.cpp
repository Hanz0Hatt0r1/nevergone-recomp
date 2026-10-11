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
        State state,
        game_scene_player_bootstrap::InitialPoint position) {
    Request result;
    result.position = position;
    if (!state.action_document_ready) {
        result.status = Status::kReadinessGateClosed;
        return result;
    }
    if (document.primary_records.empty()) {
        result.status = Status::kNoPrimaryFrames;
        return result;
    }
    if (state.current_frame_1a4 < 0 ||
        static_cast<std::size_t>(state.current_frame_1a4) >= document.primary_records.size()) {
        result.status = Status::kFrameOutOfRange;
        return result;
    }
    if (!valid_position(position)) {
        result.status = Status::kInvalidPosition;
        return result;
    }
    result.selected_primary_index = static_cast<std::size_t>(state.current_frame_1a4);
    const auto& frame = document.primary_records[result.selected_primary_index];
    if (!valid_frame_key(frame.third_string, frame.third_string_length_i32)) {
        result.status = Status::kInvalidFrameKey;
        return result;
    }

    game_scene_type0_resource::Selection resource;
    resource.kind = game_scene_type0_resource::Kind::kSpriteFrameByName;
    resource.resource_name = frame.third_string;
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
