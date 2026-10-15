#pragma once

#include <cstdint>
#include <string>

#include "game_levels_runtime_state.h"
#include "game_scene_player_motion_state.h"
#include "game_scene_player_spawn_state.h"
#include "player_control_runtime_state.h"
#include "tutorial_enemy_spawn_plan.h"

namespace nevergone::tutorial_spawn_inputs {

enum class Status {
    kReady = 0,
    kSpawnNotReady,
    kRuntimeNotReady,
    kSceneMismatch,
    kMotionNotReady,
    kMotionSpawnMismatch,
    kFacingNotEstablished,
    kPlayerFaceDirUnreviewed,
    kPlayerFaceDirStale,
    kSceneLimitsUnreviewed,
    kSceneLimitsStale,
    kNonFinitePlayerPosition,
};

// PLAYERFACEDIR is dynamic player state, but its clean-room mapping is not yet
// reviewed. A future evidence-backed provider must stamp the value with the
// exact control generation from which it was resolved.
struct ReviewedPlayerFaceDir {
    bool reviewed = false;
    std::uint64_t control_generation = 0;
    int value = 0;
};

// The original LessonLayer reads these three unsigned values from the object
// reached through GameScene +0x238 -> +0x20. Their raw offsets/ordering remain
// outside this contract until the scene-limit evidence probe is reviewed.
struct ReviewedSceneLimits {
    bool reviewed = false;
    std::uint64_t runtime_load_attempt_count = 0;
    std::string scene_guid;
    std::uint32_t vertical_edge_a = 0;
    std::uint32_t vertical_edge_b = 0;
    std::uint32_t right_edge_x = 0;
};

struct Result {
    Status status = Status::kSpawnNotReady;
    tutorial_enemy_spawn_plan::Inputs inputs{};
    std::uint64_t spawn_generation = 0;
    std::uint64_t motion_generation = 0;
    std::uint64_t control_generation = 0;
    std::uint64_t runtime_load_attempt_count = 0;

    bool ready() const { return status == Status::kReady; }
};

// Pure generation-checked adapter between already reconstructed live state and
// tutorial_enemy_spawn_plan::Inputs. It never supplies defaults for unresolved
// semantic fields: PLAYERFACEDIR and scene limits must arrive as separately
// reviewed evidence stamped to the exact live generations below.
Result resolve(
        const game_scene_player_spawn_state::Snapshot& spawn,
        const game_scene_player_motion_state::Snapshot& motion,
        const player_control_runtime_state::Snapshot& control,
        const game_levels_runtime_state::Snapshot& runtime,
        const ReviewedPlayerFaceDir& player_face_dir,
        const ReviewedSceneLimits& scene_limits);

const char* status_name(Status status);
std::string status_report(const Result& result);

}  // namespace nevergone::tutorial_spawn_inputs
