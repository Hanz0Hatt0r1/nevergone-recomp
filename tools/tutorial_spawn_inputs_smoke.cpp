#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

#include "tutorial_spawn_inputs.h"

int main() {
    namespace inputs = nevergone::tutorial_spawn_inputs;
    namespace spawn_state = nevergone::game_scene_player_spawn_state;
    namespace runtime_state = nevergone::game_levels_runtime_state;
    namespace control_contract = nevergone::player_control_contract;

    spawn_state::Snapshot spawn;
    spawn.status = spawn_state::Status::kReady;
    spawn.generation = 10;
    spawn.scene_guid = "tutorial-scene";
    spawn.player_x = 120.0f;
    spawn.player_y = 80.0f;

    nevergone::game_scene_player_motion_state::Snapshot motion;
    motion.ready = true;
    motion.generation = 20;
    motion.spawn_generation = 10;
    motion.offset_x = 30.0f;
    motion.offset_y = -5.0f;

    nevergone::player_control_runtime_state::Snapshot control;
    control.generation = 30;
    control.primary_state = control_contract::PrimaryControlState::kState12;
    control.flip_x = true;

    runtime_state::Snapshot runtime;
    runtime.status = runtime_state::LoadStatus::kReady;
    runtime.load_attempt_count = 40;
    runtime.current_scene_guid = "tutorial-scene";
    runtime.current_scene_render_queue_ready = true;

    inputs::ReviewedPlayerFaceDir face;
    face.reviewed = true;
    face.control_generation = 30;
    face.value = 7;

    inputs::ReviewedSceneLimits limits;
    limits.reviewed = true;
    limits.runtime_load_attempt_count = 40;
    limits.scene_guid = "tutorial-scene";
    limits.vertical_edge_a = 50;
    limits.vertical_edge_b = 300;
    limits.right_edge_x = 1200;

    auto result = inputs::resolve(spawn, motion, control, runtime, face, limits);
    assert(result.ready());
    assert(result.status == inputs::Status::kReady);
    assert(result.inputs.player_x == 150.0f);
    assert(result.inputs.player_y == 75.0f);
    assert(result.inputs.player_is_face);
    assert(result.inputs.player_face_dir == 7);
    assert(result.inputs.vertical_edge_a == 50);
    assert(result.inputs.vertical_edge_b == 300);
    assert(result.inputs.right_edge_x == 1200);
    assert(inputs::status_report(result).find("status: ready") != std::string::npos);

    auto bad_spawn = spawn;
    bad_spawn.status = spawn_state::Status::kIdle;
    assert(inputs::resolve(bad_spawn, motion, control, runtime, face, limits).status ==
           inputs::Status::kSpawnNotReady);

    auto bad_runtime = runtime;
    bad_runtime.current_scene_render_queue_ready = false;
    assert(inputs::resolve(spawn, motion, control, bad_runtime, face, limits).status ==
           inputs::Status::kRuntimeNotReady);

    bad_runtime = runtime;
    bad_runtime.current_scene_guid = "other-scene";
    assert(inputs::resolve(spawn, motion, control, bad_runtime, face, limits).status ==
           inputs::Status::kSceneMismatch);

    auto bad_motion = motion;
    bad_motion.ready = false;
    assert(inputs::resolve(spawn, bad_motion, control, runtime, face, limits).status ==
           inputs::Status::kMotionNotReady);

    bad_motion = motion;
    bad_motion.spawn_generation = 9;
    assert(inputs::resolve(spawn, bad_motion, control, runtime, face, limits).status ==
           inputs::Status::kMotionSpawnMismatch);

    auto bad_control = control;
    bad_control.primary_state.reset();
    assert(inputs::resolve(spawn, motion, bad_control, runtime, face, limits).status ==
           inputs::Status::kFacingNotEstablished);

    auto bad_face = face;
    bad_face.reviewed = false;
    assert(inputs::resolve(spawn, motion, control, runtime, bad_face, limits).status ==
           inputs::Status::kPlayerFaceDirUnreviewed);

    bad_face = face;
    bad_face.control_generation = 29;
    assert(inputs::resolve(spawn, motion, control, runtime, bad_face, limits).status ==
           inputs::Status::kPlayerFaceDirStale);

    auto bad_limits = limits;
    bad_limits.reviewed = false;
    assert(inputs::resolve(spawn, motion, control, runtime, face, bad_limits).status ==
           inputs::Status::kSceneLimitsUnreviewed);

    bad_limits = limits;
    bad_limits.runtime_load_attempt_count = 39;
    assert(inputs::resolve(spawn, motion, control, runtime, face, bad_limits).status ==
           inputs::Status::kSceneLimitsStale);

    bad_limits = limits;
    bad_limits.scene_guid = "old-scene";
    assert(inputs::resolve(spawn, motion, control, runtime, face, bad_limits).status ==
           inputs::Status::kSceneLimitsStale);

    bad_motion = motion;
    bad_motion.offset_x = std::numeric_limits<float>::infinity();
    assert(inputs::resolve(spawn, bad_motion, control, runtime, face, limits).status ==
           inputs::Status::kNonFinitePlayerPosition);

    std::cout << "Tutorial spawn inputs smoke: PASS\n";
    return 0;
}
