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

## Enemy 3 action selection

The imported original `enemy/enemy03/enemy_actions_1_03.plist` closes the first
Attack-wave actor's startup mapping: `start=47`, action 47 is the non-looping
`e03_e_born02b.actData` appearance clip, and action 28 is the looping
`e03_e_w01_back.actData` state clip. RevEngInstr's export of
`EnemyObject::onlineEnemyState` confirms that the native
`CCArray::objectAtIndex` receives the state stored at `EnemyObject+0x274`.
State `0x1c` therefore selects action index 28. The same verified map now
supports enemy03 states 1 (walk), 2 (run), 3/4 (attack clips), 21 (idle), and
28 (backward walk); unsupported states fail closed. The original table sets
`go_fps=30`. See
[`online-enemy-action-index-2026-10-17.md`](evidence/online-enemy-action-index-2026-10-17.md)
for the native lookup evidence.
`tutorial_enemy_action_selection` records this mapping for enemy ID 3 and fails
closed for IDs without reviewed data.

## Enemy 3 action rendering

The visual path connects each materialized ID 3 slot to the reviewed original
action documents. It loads the appearance and current-state action documents
from the imported asset tree, offsets the startup timeline by the recovered
wave spawn delay, and starts a newly selected state action at its state-change
tick. The frame's own atlas name, original sprite key, spawn position, and scene
camera translation are passed through the existing direct renderer.
Unsupported enemy IDs/states and invalid or missing action documents fail
closed instead of drawing a guessed sprite.

The live actor binding now tracks state changes independently of the immutable
spawn state. The `live_set_enemy_state` route can therefore select a new clip
and restart its local action clock. The tutorial still does not generate AI or
combat state changes itself, and the new transition path needs device
verification. One-shot state clips stop rendering when their recovered clip
duration ends; exact terminal-frame behavior remains open.

This still does not reproduce the native `EnemyActionsSystem` frame-tick
boundary or full AI/combat behavior.

## Remaining actor work

Next, drive the recovered action-state route from native-backed movement and
combat transitions, then recover and connect the original actor behavior:

1. match native action timing and facing/movement transitions;
2. route evidence-backed hit/damage/death outcomes through the same
   `(wave_generation, slot)` binding.

The tutorial still cannot advance Attack -> Block merely because three bindings
exist. Completion remains gated on the recovered `EnemyObject::isDead()`
predicate for all three materialized enemies.
