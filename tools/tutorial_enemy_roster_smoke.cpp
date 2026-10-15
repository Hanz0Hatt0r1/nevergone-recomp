#include <cassert>
#include <cmath>

#include "tutorial_enemy_roster.h"
#include "tutorial_gameplay_state.h"

namespace roster = nevergone::tutorial_enemy_roster;
namespace spawn = nevergone::tutorial_enemy_spawn_plan;
namespace tutorial = nevergone::tutorial_gameplay_state;

namespace {

struct ResolverContext {
    bool available = false;
    int base_health = 137;
    int calls = 0;
};

bool resolve_health(unsigned enemy_id, int* out_base_health, void* user_data) {
    auto* context = static_cast<ResolverContext*>(user_data);
    if (context == nullptr || out_base_health == nullptr) return false;
    ++context->calls;
    if (!context->available || enemy_id != 3u) return false;
    *out_base_health = context->base_health;
    return true;
}

bool near(float left, float right) {
    return std::fabs(left - right) < 0.0001f;
}

void enter_attack_step() {
    tutorial::reset();
    tutorial::begin_dialogue_sequence();
    assert(tutorial::observe(tutorial::Event::kDialogueSequenceComplete));
    assert(tutorial::observe(tutorial::Event::kRunPerformed));
    assert(tutorial::observe(tutorial::Event::kJumpPerformed));
    assert(tutorial::snapshot().step == tutorial::Step::kAttack);
}

}  // namespace

int main() {
    enter_attack_step();

    roster::Roster enemies;
    ResolverContext resolver;

    spawn::Inputs inputs;
    inputs.player_x = 300.0f;
    inputs.player_y = 100.0f;
    inputs.player_is_face = true;
    inputs.player_face_dir = 0x12;
    inputs.vertical_edge_a = 0;
    inputs.vertical_edge_b = 500;
    inputs.right_edge_x = 1600;

    assert(enemies.begin(spawn::Kind::kAttackWave, inputs));
    auto state = enemies.snapshot();
    assert(state.active);
    assert(state.generation == 1);
    assert(state.planned_count == 3);
    assert(state.materialized_count == 0);
    assert(!state.all_materialized);
    assert(!state.all_defeated);

    // The immediate first enemy is due, but the roster must not manufacture a
    // default HP value when imported GameLevelsEnemyData is unavailable.
    assert(enemies.advance_to(0.0f, resolve_health, &resolver));
    state = enemies.snapshot();
    assert(state.due_count == 1);
    assert(state.unresolved_health_count == 1);
    assert(state.materialized_count == 0);
    assert(!enemies.consume_all_defeated());

    resolver.available = true;
    assert(enemies.advance_to(0.0f, resolve_health, &resolver));
    state = enemies.snapshot();
    assert(state.materialized_count == 1);
    assert(state.enemies[0].request.enemy_id == 3u);
    assert(state.enemies[0].request.enemy_state == 0x1c);
    assert(near(state.enemies[0].request.x, 500.0f));
    assert(near(state.enemies[0].request.y, 100.0f));
    assert(near(state.enemies[0].health.max_health, 137.0f));
    assert(near(state.enemies[0].health.current_health, 137.0f));

    // The recovered delayed requests are scheduled independently: the third
    // slot is due at 0.5 s and the second at 1.0 s for the Attack wave.
    assert(enemies.advance_to(0.49f, resolve_health, &resolver));
    assert(enemies.snapshot().materialized_count == 1);

    assert(enemies.advance_to(0.5f, resolve_health, &resolver));
    state = enemies.snapshot();
    assert(state.materialized_count == 2);
    assert(state.enemies[2].materialized);
    assert(!state.enemies[1].materialized);
    assert(near(state.enemies[2].request.x, 750.0f));
    assert(near(state.enemies[2].request.y, 120.0f));

    assert(enemies.advance_to(1.0f, resolve_health, &resolver));
    state = enemies.snapshot();
    assert(state.materialized_count == 3);
    assert(state.all_materialized);
    assert(near(state.enemies[1].request.x, 590.0f));
    assert(near(state.enemies[1].request.y, 80.0f));
    assert(!state.all_defeated);

    // Completion tracks the recovered EnemyObject::isDead predicate rather
    // than merely observing non-positive health.
    assert(enemies.set_dead(0));
    assert(enemies.set_dead(2));
    assert(!enemies.snapshot().all_defeated);

    assert(enemies.set_enemy_state(1, 0x2c));
    assert(enemies.set_dead(1));
    state = enemies.snapshot();
    assert(state.dead_count == 2);
    assert(!state.all_defeated);
    assert(!enemies.consume_all_defeated());

    assert(enemies.set_enemy_state(1, 0x1c));
    state = enemies.snapshot();
    assert(state.dead_count == 3);
    assert(state.all_defeated);

    assert(enemies.consume_all_defeated());
    assert(!enemies.consume_all_defeated());
    state = enemies.snapshot();
    assert(!state.active);
    assert(state.completion_consumed);

    // This is the exact tutorial event boundary that advances Attack -> Block.
    assert(tutorial::observe(tutorial::Event::kTutorialEnemiesDefeated));
    assert(tutorial::snapshot().step == tutorial::Step::kBlock);

    // Absolute roster time is monotonic and cannot be rewound accidentally.
    assert(!enemies.advance_to(0.75f, resolve_health, &resolver));

    return 0;
}
