#pragma once

#include <optional>

#include <cstdint>

#include "enemy_actions_wbg_document.h"
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

// MeActionsSystem is a separate class from EnemyActionsSystem. Its initial
// frame is +0x1a4 and its parsed primary array is MeActionsData +0x34.
struct State {
    bool action_document_ready = false;
    std::int32_t current_frame_1a4 = 0;
};

struct Request {
    Status status = Status::kReadinessGateClosed;
    std::size_t selected_primary_index = 0;
    game_scene_player_bootstrap::InitialPoint position;
    std::optional<game_scene_type0_resource::Selection> resource;
};

// Converts the initial MeActionsSystem::updateData() frame lookup into a
// sprite-frame request. The caller owns the action readiness and frame state.
Request build(
        const enemy_actions_wbg_document::Document& document,
        State state,
        game_scene_player_bootstrap::InitialPoint position);

const char* status_name(Status status);

}  // namespace nevergone::game_scene_player_frame_request
