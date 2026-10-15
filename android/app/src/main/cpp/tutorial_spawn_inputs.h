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

// PLAYERFACEDIR is derived from the recovered MEPlayer +0x12c facing contract.
// The provider stamps it with the exact control generation from which it was
// resolved so stale facing state cannot cross input generations.
struct ReviewedPlayerFaceDir {
    bool reviewed = false;
    std::uint64_t control_generation = 0;
    int value = 0;
};

// The original LessonLayer reads these unsigned values from the current
// GameScenePortNodeData reached through GameScene +0x238 -> +0x20. Exact ARMv7
// evidence maps third/fourth/fifth serialized PortNode uint32s to
// vertical_edge_a / vertical_edge_b / right_edge_x respectively. The live
// provider stamps those parsed values to one GameLevels load attempt and scene.
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

// Pure generation-checked adapter between reconstructed live state and
// tutorial_enemy_spawn_plan::Inputs. It never supplies defaults: both reviewed
// provider outputs must belong to the exact live generations below.
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
