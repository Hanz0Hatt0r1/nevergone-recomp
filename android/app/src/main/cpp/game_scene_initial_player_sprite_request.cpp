#include "game_scene_initial_player_sprite_request.h"

namespace nevergone::game_scene_initial_player_sprite_request {

Request build(
        const enemy_actions_wbg_document::Document& document,
        game_scene_player_bootstrap::InitialPoint player_position) {
    Request result;
    const auto state = me_actions_runtime_state::initial_play_state();
    result.frame_request = game_scene_player_frame_request::build(
            document,
            state,
            player_position);
    if (result.frame_request.status != game_scene_player_frame_request::Status::kReady ||
            !result.frame_request.resource.has_value() ||
            !result.frame_request.lookup.current_frame_in_range) {
        result.status = Status::kFrameRequestFailed;
        return result;
    }

    const std::size_t index = result.frame_request.lookup.selected_primary_index;
    if (index >= document.primary_records.size()) {
        result.status = Status::kFrameRequestFailed;
        return result;
    }

    result.sprite_transform = me_actions_initial_sprite_transform::build(
            document.primary_records[index]);
    if (!result.sprite_transform.has_value()) {
        result.status = Status::kInvalidSpriteTransform;
        return result;
    }

    result.status = Status::kReady;
    return result;
}

const char* status_name(Status status) {
    switch (status) {
        case Status::kFrameRequestFailed: return "frame-request-failed";
        case Status::kInvalidSpriteTransform: return "invalid-sprite-transform";
        case Status::kReady: return "ready";
    }
    return "unknown";
}

}  // namespace nevergone::game_scene_initial_player_sprite_request
