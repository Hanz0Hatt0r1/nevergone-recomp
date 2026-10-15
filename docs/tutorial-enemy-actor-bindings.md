# Tutorial enemy actor bindings

The first Attack-wave roster now has enough reviewed data to materialize slots
from the imported tutorial scene, but a roster slot is not yet a rendered or
AI-driven `EnemyObject`. `tutorial_enemy_actor_bindings` is the narrow identity
boundary between those two layers.

## Binding identity

A binding is created only after a roster slot is actually materialized. It
retains:

```text
slot
wave_generation
enemy_id
initial_enemy_state
base_health
spawn_x
spawn_y
```

All values come directly from the materialized roster slot. The binding layer
never chooses a sprite, action-data file, AI state, hit box, damage amount, or
death result.

`(wave_generation, slot)` is the stable live identity. A slot index by itself is
not accepted because tutorial waves reuse the same small slot range.

## Monotonic materialization

The recovered Attack wave materializes incrementally at its fixed logical
clock:

- slot 0 immediately;
- slot 2 at the first tick reaching 0.5 seconds;
- slot 1 at 1.0 second.

Bindings mirror that exact materialization. A planned but not-yet-materialized
slot has no actor identity and cannot receive combat callbacks.

Within one wave, published materialization is required to be monotonic. If a
slot disappears or its immutable identity/initial geometry changes after a
binding was published, synchronization fails closed instead of silently
rebinding the actor.

## Live gate integration

`tutorial_attack_live_gate::Gate` owns the binding store and synchronizes it
after every Attack-wave runtime tick. The gate routes damage, explicit death,
enemy-state, and force-no-dead callbacks only when the binding store accepts the
exact `(slot, wave_generation)` pair.

This adds an independent guard above the roster's own generation/materialization
checks and creates the production handoff future enemy actors can retain.

A scene reload, gameplay step change, or explicit gate reset clears the binding
store together with the Attack wave. Late callbacks from the old scene then fail
before touching roster health.

## Diagnostics

The existing Tutorial Attack live-gate report now appends a `Tutorial enemy
actor bindings` section containing the active wave generation and each bound
slot's recovered ID/state/HP/spawn coordinates. This makes device evidence able
to distinguish:

- planned roster enemies;
- materialized roster enemies;
- published actor identities.

## Remaining actor work

This boundary intentionally stops before visual or behavioral invention. The
next reconstruction slice is to connect each active binding to the already
recovered EnemyActions pipeline:

1. recover the original enemy-ID -> action/resource data selection used by
   `EnemyObject::loadEnemyObjectFile()` / EnemyActions setup;
2. load the corresponding imported EnemyActions document;
3. produce a renderer-facing current frame/transform at the binding spawn
   position;
4. route evidence-backed hit/damage/death outcomes back through the same
   `(wave_generation, slot)` binding.

The tutorial still cannot advance Attack -> Block merely because three bindings
exist. Completion remains gated on the recovered `EnemyObject::isDead()`
predicate for all three materialized enemies.
