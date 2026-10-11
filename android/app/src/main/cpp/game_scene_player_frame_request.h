#pragma once

#include <optional>

#include "enemy_actions_runtime_state.h"
#include "game_scene_player_bootstrap.h"
#include "game_scene_type0_resource.h"

namespace nevergone::game_scene_player_frame_request {

enum class Status {
    kReadinessGateClosed = 0,
    kNoPrimaryFrames,
    kFrameOutOfRange,
    kInvalidPosition,
    kInvalidFrameKey,
    kReady,
};

struct Request {
    Status status = Status::kReadinessGateClosed;
    enemy_actions_runtime_state::UpdateDataFrameLookupResult lookup;
    game_scene_player_bootstrap::InitialPoint position;
    std::optional<game_scene_type0_resource::Selection> resource;
};

// Converts the evidence-backed EnemyActionsSystem::updateData() frame lookup
// into the exact sprite-frame resource request consumed by the existing
// GameScene atlas staging pipeline. The caller owns the runtime State; this
// bridge intentionally does not guess readiness/current-frame transitions.
Request build(
        const enemy_actions_wbg_document::Document& document,
        enemy_actions_runtime_state::State state,
        game_scene_player_bootstrap::InitialPoint position);

const char* status_name(Status status);

}  // namespace nevergone::game_scene_player_frame_request
