#pragma once

#include <optional>

#include "game_scene_player_frame_request.h"
#include "me_actions_initial_sprite_transform.h"

namespace nevergone::game_scene_initial_player_sprite_request {

enum class Status {
    kFrameRequestFailed = 0,
    kInvalidSpriteTransform,
    kReady,
};

struct Request {
    Status status = Status::kFrameRequestFailed;
    game_scene_player_frame_request::Request frame_request;
    std::optional<me_actions_initial_sprite_transform::Plan> sprite_transform;
};

// Composes the exact first MeActionsSystem play state with the already recovered
// player node position and the initial body-sprite transform. This deliberately
// models only the first visible frame; later animation frames require mutable
// MeActions runtime offsets rather than reusing these init-time constants.
Request build(
        const enemy_actions_wbg_document::Document& document,
        game_scene_player_bootstrap::InitialPoint player_position);

const char* status_name(Status status);

}  // namespace nevergone::game_scene_initial_player_sprite_request
