#pragma once

#include <optional>

#include "game_scene_player_bootstrap.h"
#include "game_scene_type0_resource.h"
#include "me_actions_runtime_state.h"

namespace nevergone::game_scene_player_frame_request {

enum class Status {
    kNoPrimaryFrames = 0,
    kFrameOutOfRange,
    kInvalidPosition,
    kInvalidFrameKey,
    kReady,
};

struct Request {
    Status status = Status::kNoPrimaryFrames;
    me_actions_runtime_state::UpdateDataFrameLookupResult lookup;
    game_scene_player_bootstrap::InitialPoint position;
    std::optional<game_scene_type0_resource::Selection> resource;
};

// Converts the evidence-backed MeActionsSystem player-frame lookup into the
// exact sprite-frame resource request consumed by the existing GameScene atlas
// staging pipeline. The caller owns the MeActions runtime state; this bridge
// does not reuse EnemyActionsSystem offsets/readiness semantics.
Request build(
        const enemy_actions_wbg_document::Document& document,
        me_actions_runtime_state::State state,
        game_scene_player_bootstrap::InitialPoint position);

const char* status_name(Status status);

}  // namespace nevergone::game_scene_player_frame_request
