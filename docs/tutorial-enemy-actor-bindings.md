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
`e03_e_w01_back.actData` state clip. The tutorial roster requests state `0x1c`
(28); `EnemyObject::onlineEnemyState` selects the action associated with its
requested state. The current implementation treats state `0x1c` as action
index 28, which is a candidate mapping: the table confirms the clip, but the
direct native state-to-action index lookup has not yet been traced. The original
table sets `go_fps=30`.
`tutorial_enemy_action_selection` records this mapping for enemy ID 3 and fails
closed for IDs without reviewed data.

## Enemy 3 action rendering

The visual path now connects each materialized ID 3 slot to the reviewed
original action documents. It loads the appearance and state-action documents
from the imported asset tree, offsets each slot's local timeline by the
recovered wave spawn delay, and selects the appearance frame before
transitioning to the looping action for its initial state. The frame's own
atlas name, original sprite key, spawn position, and scene camera translation
are passed through the existing direct renderer. Unsupported enemy IDs,
unmapped states, and invalid/missing action documents fail closed instead of
drawing a guessed sprite.

This renders the recovered startup/state animation, but it does not yet
reproduce the native `EnemyActionsSystem` tick boundary or full AI/combat
behavior. Device verification of the action transition in the tutorial wave is
still required.

## Remaining actor work

Next, confirm the state-to-action index against a live original trace, compare
the appearance-to-state-action sequence, then recover and connect the original
actor behavior:

1. match native action timing and facing/movement transitions;
2. route evidence-backed hit/damage/death outcomes through the same
   `(wave_generation, slot)` binding.

The tutorial still cannot advance Attack -> Block merely because three bindings
exist. Completion remains gated on the recovered `EnemyObject::isDead()`
predicate for all three materialized enemies.
