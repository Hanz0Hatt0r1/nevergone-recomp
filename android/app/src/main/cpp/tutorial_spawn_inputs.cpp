#include "tutorial_spawn_inputs.h"

#include <cmath>
#include <sstream>

namespace nevergone::tutorial_spawn_inputs {

Result resolve(
        const game_scene_player_spawn_state::Snapshot& spawn,
        const game_scene_player_motion_state::Snapshot& motion,
        const player_control_runtime_state::Snapshot& control,
        const game_levels_runtime_state::Snapshot& runtime,
        const ReviewedPlayerFaceDir& player_face_dir,
        const ReviewedSceneLimits& scene_limits) {
    Result result;
    result.spawn_generation = spawn.generation;
    result.motion_generation = motion.generation;
    result.control_generation = control.generation;
    result.runtime_load_attempt_count = runtime.load_attempt_count;

    if (spawn.status != game_scene_player_spawn_state::Status::kReady) {
        result.status = Status::kSpawnNotReady;
        return result;
    }
    if (runtime.status != game_levels_runtime_state::LoadStatus::kReady ||
            !runtime.current_scene_render_queue_ready) {
        result.status = Status::kRuntimeNotReady;
        return result;
    }
    if (spawn.scene_guid.empty() || runtime.current_scene_guid.empty() ||
            spawn.scene_guid != runtime.current_scene_guid) {
        result.status = Status::kSceneMismatch;
        return result;
    }
    if (!motion.ready) {
        result.status = Status::kMotionNotReady;
        return result;
    }
    if (motion.spawn_generation != spawn.generation) {
        result.status = Status::kMotionSpawnMismatch;
        return result;
    }
    if (!control.primary_state.has_value()) {
        result.status = Status::kFacingNotEstablished;
        return result;
    }
    if (!player_face_dir.reviewed) {
        result.status = Status::kPlayerFaceDirUnreviewed;
        return result;
    }
    if (player_face_dir.control_generation != control.generation) {
        result.status = Status::kPlayerFaceDirStale;
        return result;
    }
    if (!scene_limits.reviewed) {
        result.status = Status::kSceneLimitsUnreviewed;
        return result;
    }
    if (scene_limits.runtime_load_attempt_count != runtime.load_attempt_count ||
            scene_limits.scene_guid != runtime.current_scene_guid) {
        result.status = Status::kSceneLimitsStale;
        return result;
    }

    const float player_x = spawn.player_x + motion.offset_x;
    const float player_y = spawn.player_y + motion.offset_y;
    if (!std::isfinite(player_x) || !std::isfinite(player_y)) {
        result.status = Status::kNonFinitePlayerPosition;
        return result;
    }

    result.inputs.player_x = player_x;
    result.inputs.player_y = player_y;
    // The retained Controls path stores the same bool passed to
    // MEPlayer::filpPlayer(bool, ...) and CCSprite::setFlipX(bool). The
    // separate PLAYERFACEDIR integer remains supplied above by reviewed
    // evidence rather than being inferred from this bool.
    result.inputs.player_is_face = control.flip_x;
    result.inputs.player_face_dir = player_face_dir.value;
    result.inputs.vertical_edge_a = scene_limits.vertical_edge_a;
    result.inputs.vertical_edge_b = scene_limits.vertical_edge_b;
    result.inputs.right_edge_x = scene_limits.right_edge_x;
    result.status = Status::kReady;
    return result;
}

const char* status_name(Status status) {
    switch (status) {
        case Status::kReady: return "ready";
        case Status::kSpawnNotReady: return "spawn-not-ready";
        case Status::kRuntimeNotReady: return "runtime-not-ready";
        case Status::kSceneMismatch: return "scene-mismatch";
        case Status::kMotionNotReady: return "motion-not-ready";
        case Status::kMotionSpawnMismatch: return "motion-spawn-mismatch";
        case Status::kFacingNotEstablished: return "facing-not-established";
        case Status::kPlayerFaceDirUnreviewed: return "player-face-dir-unreviewed";
        case Status::kPlayerFaceDirStale: return "player-face-dir-stale";
        case Status::kSceneLimitsUnreviewed: return "scene-limits-unreviewed";
        case Status::kSceneLimitsStale: return "scene-limits-stale";
        case Status::kNonFinitePlayerPosition: return "non-finite-player-position";
    }
    return "unknown";
}

std::string status_report(const Result& result) {
    std::ostringstream out;
    out << "Tutorial spawn inputs\n";
    out << "status: " << status_name(result.status) << "\n";
    out << "spawn generation: " << result.spawn_generation << "\n";
    out << "motion generation: " << result.motion_generation << "\n";
    out << "control generation: " << result.control_generation << "\n";
    out << "runtime load attempt: " << result.runtime_load_attempt_count << "\n";
    if (result.ready()) {
        out << "player position: " << result.inputs.player_x << "," << result.inputs.player_y << "\n";
        out << "player is face: " << (result.inputs.player_is_face ? "true" : "false") << "\n";
        out << "player face dir: " << result.inputs.player_face_dir << "\n";
        out << "scene limits: "
            << result.inputs.vertical_edge_a << ","
            << result.inputs.vertical_edge_b << ","
            << result.inputs.right_edge_x << "\n";
    }
    return out.str();
}

}  // namespace nevergone::tutorial_spawn_inputs
