#include <cassert>
#include <iostream>
#include <vector>

#include "tutorial_attack_live_gate.h"
#include "tutorial_player_face_dir_provider.h"

int main() {
    namespace gate_ns = nevergone::tutorial_attack_live_gate;
    namespace face_ns = nevergone::tutorial_player_face_dir_provider;
    namespace gameplay_ns = nevergone::tutorial_gameplay_state;
    namespace spawn_ns = nevergone::game_scene_player_spawn_state;
    namespace levels_ns = nevergone::game_levels_runtime_state;
    namespace control_ns = nevergone::player_control_contract;
    namespace mapping_ns = nevergone::game_levels_enemy_record_mapping;
    namespace inputs_ns = nevergone::tutorial_spawn_inputs;
    namespace attack_ns = nevergone::tutorial_attack_wave_runtime;

    gameplay_ns::Snapshot gameplay;
    gameplay.step = gameplay_ns::Step::kAttack;
    gameplay.active = true;

    spawn_ns::Snapshot spawn;
    spawn.status = spawn_ns::Status::kReady;
    spawn.generation = 10;
    spawn.scene_guid = "tutorial-scene";
    spawn.player_x = 300.0f;
    spawn.player_y = 100.0f;

    nevergone::game_scene_player_motion_state::Snapshot motion;
    motion.ready = true;
    motion.generation = 20;
    motion.spawn_generation = 10;

    nevergone::player_control_runtime_state::Snapshot control;
    control.generation = 30;
    control.primary_state = control_ns::PrimaryControlState::kState12;
    control.flip_x = true;

    levels_ns::Snapshot levels;
    levels.status = levels_ns::LoadStatus::kReady;
    levels.load_attempt_count = 40;
    levels.current_scene_guid = "tutorial-scene";
    levels.current_scene_render_queue_ready = true;

    std::vector<nevergone::game_levels_global_section::EnemyRecord> records(1);
    records[0].first_i32 = 3;
    records[0].second_i32 = 137;

    gate_ns::Gate gate;

    // Pure Gate remains fail-closed until an evidence provider supplies the
    // per-generation facing value.
    assert(gate.tick(100, gameplay, spawn, motion, control, levels, records));
    auto state = gate.snapshot();
    assert(state.spawn_inputs.status == inputs_ns::Status::kPlayerFaceDirUnreviewed);
    assert(state.attack.status == attack_ns::Status::kWaitingForSpawnInputs);
    assert(!state.attack.wave_started);

    // Production now derives this from the recovered shared MEPlayer +0x12c
    // contract. Once injected, scene limits become the next unresolved gate.
    const auto face = face_ns::resolve(control);
    assert(face.ready());
    assert(face.face_dir.value == 1);
    gate.set_player_face_dir_evidence(face.face_dir);
    assert(gate.tick(100, gameplay, spawn, motion, control, levels, records));
    state = gate.snapshot();
    assert(state.spawn_inputs.status == inputs_ns::Status::kSceneLimitsUnreviewed);
    assert(!state.attack.wave_started);

    gate_ns::ReviewBundle review;
    review.player_face_dir = face.face_dir;
    review.scene_limits.reviewed = true;
    review.scene_limits.runtime_load_attempt_count = 40;
    review.scene_limits.scene_guid = "tutorial-scene";
    review.scene_limits.vertical_edge_a = 0;
    review.scene_limits.vertical_edge_b = 500;
    review.scene_limits.right_edge_x = 1600;

    // Candidate-only enemy mappings remain unusable in production.
    review.enemy_mapping.identity_field = mapping_ns::IntegerField::kFirstI32;
    review.enemy_mapping.base_health_field = mapping_ns::IntegerField::kSecondI32;
    review.enemy_mapping.review_status = mapping_ns::ReviewStatus::kCandidate;
    gate.set_review_bundle(review);
    assert(gate.tick(100, gameplay, spawn, motion, control, levels, records));
    state = gate.snapshot();
    assert(state.spawn_inputs.ready());
    assert(!state.enemy_mapping_usable);
    assert(state.attack.status == attack_ns::Status::kWaitingForBaseHealth);
    assert(!state.attack.wave_started);

    // The same structurally valid mapping becomes usable only after the
    // independent review boundary explicitly promotes it.
    review.enemy_mapping.review_status = mapping_ns::ReviewStatus::kReviewed;
    gate.set_review_bundle(review);
    assert(gate.tick(100, gameplay, spawn, motion, control, levels, records));
    state = gate.snapshot();
    assert(state.enemy_mapping_usable);
    assert(state.enemy_record_count == 1);
    assert(state.attack.status == attack_ns::Status::kActive);
    assert(state.attack.roster.materialized_count == 1);
    const std::uint64_t wave = state.attack.wave_generation;
    assert(wave != 0);

    assert(gate.tick(118, gameplay, spawn, motion, control, levels, records));
    assert(gate.snapshot().attack.roster.materialized_count == 2);
    assert(gate.tick(135, gameplay, spawn, motion, control, levels, records));
    assert(gate.snapshot().attack.roster.materialized_count == 3);

    assert(gate.set_dead(0, wave));
    assert(gate.set_dead(1, wave));
    assert(gate.set_dead(2, wave));
    assert(gate.tick(136, gameplay, spawn, motion, control, levels, records));
    state = gate.snapshot();
    assert(state.attack.completion_pending);
    assert(state.completion_publish_count == 0);
    assert(gate.consume_completion_after_publish());
    assert(!gate.consume_completion_after_publish());
    assert(gate.snapshot().completion_publish_count == 1);

    // A control-generation change receives a fresh provider token. Reusing the
    // old token remains stale and fails closed until the provider is refreshed.
    gate.reset();
    control.generation = 31;
    review.player_face_dir = face.face_dir;
    gate.set_review_bundle(review);
    assert(gate.tick(200, gameplay, spawn, motion, control, levels, records));
    state = gate.snapshot();
    assert(state.spawn_inputs.status == inputs_ns::Status::kPlayerFaceDirStale);
    assert(!state.attack.wave_started);

    const auto refreshed_face = face_ns::resolve(control);
    assert(refreshed_face.ready());
    assert(refreshed_face.face_dir.control_generation == 31);
    gate.set_player_face_dir_evidence(refreshed_face.face_dir);
    assert(gate.tick(200, gameplay, spawn, motion, control, levels, records));
    assert(gate.snapshot().spawn_inputs.ready());
    const std::uint64_t old_wave = gate.snapshot().attack.wave_generation;
    assert(old_wave != 0);

    // A new imported scene load invalidates the old wave even when the
    // gameplay step has not yet moved away from Attack.
    levels.load_attempt_count = 41;
    levels.current_scene_guid = "another-scene";
    levels.current_scene_render_queue_ready = false;
    assert(gate.tick(201, gameplay, spawn, motion, control, levels, records));
    assert(!gate.snapshot().attack.wave_started);
    assert(!gate.set_dead(0, old_wave));
    assert(!gate.apply_damage(0, 1.0f, old_wave));
    assert(!gate.consume_completion_after_publish());

    // The old reviewed scene limits and old spawn GUID must not allow a
    // wave to be started in the replacement scene.
    levels.status = levels_ns::LoadStatus::kReady;
    levels.current_scene_render_queue_ready = true;
    assert(gate.tick(202, gameplay, spawn, motion, control, levels, records));
    assert(!gate.snapshot().attack.wave_started);
    assert(!gate.set_dead(0, old_wave));

    std::cout << "Tutorial Attack live gate smoke: PASS\n";
    return 0;
}
