#include <cassert>
#include <cmath>

#include "tutorial_enemy_actor_bindings.h"

namespace bindings = nevergone::tutorial_enemy_actor_bindings;
namespace attack = nevergone::tutorial_attack_wave_runtime;

namespace {

void materialize(
        attack::Snapshot* state,
        std::size_t slot,
        unsigned enemy_id,
        int enemy_state,
        float hp,
        float x,
        float y) {
    assert(state != nullptr);
    assert(slot < state->roster.enemies.size());
    auto& enemy = state->roster.enemies[slot];
    enemy.planned = true;
    enemy.due = true;
    enemy.materialized = true;
    enemy.request.enemy_id = enemy_id;
    enemy.request.enemy_state = enemy_state;
    enemy.request.x = x;
    enemy.request.y = y;
    enemy.health.max_health = hp;
    enemy.health.current_health = hp;
}

attack::Snapshot active_wave(std::uint64_t generation) {
    attack::Snapshot state;
    state.status = attack::Status::kActive;
    state.wave_started = true;
    state.wave_generation = generation;
    state.roster.generation = generation;
    state.roster.active = true;
    state.roster.planned_count = 3;
    for (std::size_t i = 0; i < 3; ++i) {
        state.roster.enemies[i].planned = true;
    }
    return state;
}

}  // namespace

int main() {
    bindings::Store store;
    auto wave = active_wave(7);

    // A planned-but-not-materialized slot cannot acquire an actor identity.
    assert(store.sync(wave));
    auto state = store.snapshot();
    assert(state.wave_bound);
    assert(state.wave_generation == 7);
    assert(state.active_count == 0);
    assert(!store.accepts(0, 7));

    materialize(&wave, 0, 3u, 0x1c, 137.0f, 500.0f, 100.0f);
    assert(store.sync(wave));
    state = store.snapshot();
    assert(state.active_count == 1);
    assert(state.bindings[0].active);
    assert(state.bindings[0].slot == 0);
    assert(state.bindings[0].wave_generation == 7);
    assert(state.bindings[0].enemy_id == 3u);
    assert(state.bindings[0].initial_enemy_state == 0x1c);
    assert(state.bindings[0].base_health == 137.0f);
    assert(state.bindings[0].spawn_x == 500.0f);
    assert(state.bindings[0].spawn_y == 100.0f);
    assert(store.accepts(0, 7));
    assert(!store.accepts(0, 6));
    assert(!store.accepts(1, 7));

    // Re-syncing unchanged roster state is stable and does not churn bindings.
    const auto unchanged_generation = state.generation;
    assert(store.sync(wave));
    assert(store.snapshot().generation == unchanged_generation);

    materialize(&wave, 2, 3u, 0x1c, 137.0f, 750.0f, 120.0f);
    assert(store.sync(wave));
    assert(store.snapshot().active_count == 2);
    assert(store.accepts(2, 7));

    materialize(&wave, 1, 3u, 0x1c, 137.0f, 590.0f, 80.0f);
    assert(store.sync(wave));
    assert(store.snapshot().active_count == 3);

    // Within one wave materialization is monotonic. Losing an already-published
    // slot fails closed instead of silently rebinding its stable identity.
    auto regressed = wave;
    regressed.roster.enemies[1].materialized = false;
    assert(!store.sync(regressed));
    assert(store.accepts(1, 7));

    // Corrupt actor source geometry/health never produces a binding.
    auto next = active_wave(8);
    materialize(&next, 0, 3u, 0x1c, 137.0f, 500.0f, 100.0f);
    next.roster.enemies[0].request.x = std::nanf("");
    assert(!store.sync(next));
    assert(!store.accepts(0, 7));
    assert(!store.accepts(0, 8));

    // A valid new wave starts with an independent identity generation and
    // atomically invalidates every prior binding.
    next = active_wave(8);
    materialize(&next, 0, 3u, 0x1c, 200.0f, 400.0f, 90.0f);
    assert(store.sync(next));
    assert(store.accepts(0, 8));
    assert(!store.accepts(0, 7));
    assert(store.snapshot().active_count == 1);

    attack::Snapshot idle;
    assert(store.sync(idle));
    state = store.snapshot();
    assert(!state.wave_bound);
    assert(state.wave_generation == 0);
    assert(state.active_count == 0);
    assert(!store.accepts(0, 8));

    return 0;
}
