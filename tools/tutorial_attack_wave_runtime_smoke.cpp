#include <cassert>
#include <iostream>

#include "tutorial_attack_wave_runtime.h"

namespace {

struct HealthContext {
    bool available = false;
    int hp = 100;
    int calls = 0;
};

bool resolve_health(unsigned enemy_id, int* out_base_health, void* user_data) {
    auto* context = static_cast<HealthContext*>(user_data);
    if (context == nullptr || out_base_health == nullptr) return false;
    ++context->calls;
    if (!context->available || enemy_id != 3 || context->hp <= 0) return false;
    *out_base_health = context->hp;
    return true;
}

nevergone::tutorial_gameplay_state::Snapshot gameplay(
        nevergone::tutorial_gameplay_state::Step step) {
    nevergone::tutorial_gameplay_state::Snapshot out;
    out.step = step;
    out.active = step != nevergone::tutorial_gameplay_state::Step::kInactive &&
        step != nevergone::tutorial_gameplay_state::Step::kComplete;
    return out;
}

nevergone::tutorial_spawn_inputs::Result ready_inputs(float player_x) {
    nevergone::tutorial_spawn_inputs::Result out;
    out.status = nevergone::tutorial_spawn_inputs::Status::kReady;
    out.inputs.player_x = player_x;
    out.inputs.player_y = 100.0f;
    out.inputs.player_is_face = true;
    out.inputs.player_face_dir = 7;
    out.inputs.vertical_edge_a = 0;
    out.inputs.vertical_edge_b = 500;
    out.inputs.right_edge_x = 1400;
    return out;
}

}  // namespace

int main() {
    namespace runtime_ns = nevergone::tutorial_attack_wave_runtime;
    namespace gameplay_ns = nevergone::tutorial_gameplay_state;
    namespace spawn_inputs_ns = nevergone::tutorial_spawn_inputs;
    namespace spawn_plan_ns = nevergone::tutorial_enemy_spawn_plan;

    runtime_ns::Runtime runtime;
    HealthContext health;

    auto inputs = ready_inputs(100.0f);
    assert(runtime.tick(10, gameplay(gameplay_ns::Step::kRun), inputs, resolve_health, &health));
    assert(runtime.snapshot().status == runtime_ns::Status::kIdle);

    auto blocked_inputs = inputs;
    blocked_inputs.status = spawn_inputs_ns::Status::kSceneLimitsUnreviewed;
    assert(runtime.tick(
        20, gameplay(gameplay_ns::Step::kAttack), blocked_inputs, resolve_health, &health));
    auto state = runtime.snapshot();
    assert(state.status == runtime_ns::Status::kWaitingForSpawnInputs);
    assert(!state.wave_started);
    assert(state.roster.planned_count == 0);

    // Geometry must not freeze while imported HP is unresolved. The player can
    // move and the later ready input snapshot must be the one used at begin().
    inputs = ready_inputs(220.0f);
    assert(runtime.tick(21, gameplay(gameplay_ns::Step::kAttack), inputs, resolve_health, &health));
    state = runtime.snapshot();
    assert(state.status == runtime_ns::Status::kWaitingForBaseHealth);
    assert(!state.wave_started);
    assert(state.roster.planned_count == 0);

    const auto latest_inputs = ready_inputs(360.0f);
    const auto expected_plan = spawn_plan_ns::build(
        spawn_plan_ns::Kind::kAttackWave, latest_inputs.inputs);
    health.available = true;
    assert(runtime.tick(
        100, gameplay(gameplay_ns::Step::kAttack), latest_inputs, resolve_health, &health));
    state = runtime.snapshot();
    assert(state.status == runtime_ns::Status::kActive);
    assert(state.wave_started);
    assert(state.start_game_tick == 100);
    assert(state.roster.planned_count == 3);
    assert(state.roster.materialized_count == 1);
    assert(state.wave_generation == state.roster.generation);
    assert(state.wave_generation != 0);
    const std::uint64_t first_wave = state.wave_generation;
    assert(state.roster.enemies[0].request.x == expected_plan.enemies[0].x);
    assert(state.roster.enemies[0].request.y == expected_plan.enemies[0].y);
    assert(state.roster.enemies[0].request.player_face_dir == 7);

    // 0.5 s lies between fixed ticks 17 and 18 at 35 Hz. The second enemy is
    // therefore first due at tick 18; the 1.0 s enemy is due exactly at 35.
    assert(runtime.tick(
        117, gameplay(gameplay_ns::Step::kAttack), latest_inputs, resolve_health, &health));
    assert(runtime.snapshot().roster.materialized_count == 1);
    assert(runtime.tick(
        118, gameplay(gameplay_ns::Step::kAttack), latest_inputs, resolve_health, &health));
    assert(runtime.snapshot().roster.materialized_count == 2);
    assert(runtime.tick(
        134, gameplay(gameplay_ns::Step::kAttack), latest_inputs, resolve_health, &health));
    assert(runtime.snapshot().roster.materialized_count == 2);
    assert(runtime.tick(
        135, gameplay(gameplay_ns::Step::kAttack), latest_inputs, resolve_health, &health));
    assert(runtime.snapshot().roster.materialized_count == 3);

    assert(runtime.set_dead(0, first_wave));
    assert(runtime.set_dead(1, first_wave));
    assert(runtime.set_dead(2, first_wave));
    assert(runtime.tick(
        136, gameplay(gameplay_ns::Step::kAttack), latest_inputs, resolve_health, &health));
    state = runtime.snapshot();
    assert(state.status == runtime_ns::Status::kCompletionPending);
    assert(state.wave_completed);
    assert(state.completion_pending);
    assert(state.roster.all_defeated);
    assert(state.roster.completion_consumed);

    assert(runtime.consume_completion());
    assert(!runtime.consume_completion());
    state = runtime.snapshot();
    assert(state.status == runtime_ns::Status::kCompleted);
    assert(state.wave_completed);
    assert(!state.completion_pending);

    // The outer state machine may consume the edge and not change step until
    // the next frame. That frame must stay completed rather than touching the
    // now-inactive roster again.
    assert(runtime.tick(
        137, gameplay(gameplay_ns::Step::kAttack), latest_inputs, resolve_health, &health));
    assert(runtime.snapshot().status == runtime_ns::Status::kCompleted);

    // Once Attack really advances to Block, all wave-local state is discarded.
    assert(runtime.tick(
        138, gameplay(gameplay_ns::Step::kBlock), latest_inputs, resolve_health, &health));
    state = runtime.snapshot();
    assert(state.status == runtime_ns::Status::kIdle);
    assert(!state.wave_started);
    assert(!state.wave_completed);
    assert(state.wave_generation == 0);
    assert(state.roster.planned_count == 0);

    // Starting another Attack wave produces a different token. A late callback
    // carrying the completed first-wave token cannot touch the reused slot.
    assert(runtime.tick(
        200, gameplay(gameplay_ns::Step::kAttack), latest_inputs, resolve_health, &health));
    state = runtime.snapshot();
    const std::uint64_t second_wave = state.wave_generation;
    assert(second_wave != 0);
    assert(second_wave != first_wave);
    assert(!runtime.set_dead(0, first_wave));
    assert(!runtime.apply_damage(0, 50.0f, first_wave));
    assert(runtime.snapshot().roster.dead_count == 0);
    assert(runtime.apply_damage(0, 50.0f, second_wave));

    // Fixed-clock regression fails closed and never creates a completion edge.
    runtime_ns::Runtime regression;
    assert(regression.tick(
        300, gameplay(gameplay_ns::Step::kAttack), latest_inputs, resolve_health, &health));
    assert(!regression.tick(
        299, gameplay(gameplay_ns::Step::kAttack), latest_inputs, resolve_health, &health));
    assert(regression.snapshot().status == runtime_ns::Status::kClockRegression);
    assert(!regression.consume_completion());

    std::cout << "Tutorial attack wave runtime smoke: PASS\n";
    return 0;
}
