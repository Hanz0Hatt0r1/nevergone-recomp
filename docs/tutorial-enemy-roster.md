# Tutorial enemy roster

`tutorial_enemy_roster` connects the recovered `LessonLayer` spawn plan to the
recovered `EnemyObject` health/death contract without inventing HP or completion
signals.

## Recovered Attack wave

`tutorial_enemy_spawn_plan` remains the authority for tutorial enemy IDs,
states, positions and delays. The first Attack lesson creates three enemy ID `3`
requests in state `0x1c`:

- one immediately;
- one after `0.5 s`;
- one after `1.0 s`.

The same recovered plan uses enemy ID `3` for later Block/Execution requests and
ID `0x15` (`21`) for the AirAttack enemy.

## Imported base health is now reviewed

Exact original ARMv7 consumers close the `LoadGL_Global` enemy record mapping:

```text
first_i32  -> GameLevelsEnemyData +0x14 -> enemy_id
second_i32 -> GameLevelsEnemyData +0x18 -> base_health
second_i32 -> GameLevelsEnemyData +0x1c -> total-blood mirror
```

`game_levels_enemy_record_mapping::reviewed_original_mapping()` supplies this
mapping to the live Attack gate. The resolver still requires exactly one record
for the requested enemy ID and positive base HP. Missing/duplicate ID rows or
non-positive HP leave materialization blocked rather than substituting a
fallback constant.

The exact-binary provenance and verifier are documented in
`docs/evidence/enemy-record-identity-health-2026-10-15.md`.

Legacy runtime/Ghidra cross-evidence tools remain useful for independent
corroboration, but their output stays candidate-only and is not what promotes
the production mapping.

## Health/death semantics

`enemy_health_state` mirrors the recovered `EnemyObject::isDead()` predicate. A
slot is not considered dead merely because HP is non-positive when enemy state
`0x0f`/`0x2c` or the force-no-dead flag suppresses death.

The roster therefore never completes from an A-button press or elapsed time.
Damage/death/state must arrive through explicit live callbacks.

## Wave-generation safety

Each mutation requires the generation returned by `Roster::snapshot()` after
`begin()`. This applies to scheduled materialization, damage, death, enemy-state
changes, no-death overrides and `consume_all_defeated()`.

A delayed callback from an old wave cannot mutate a new wave that reuses the
same slot index. `reset()` invalidates outstanding tokens without reusing the
prior generation. Non-finite or negative external damage is rejected.

## Completion boundary

`consume_all_defeated()` exposes a one-shot edge only after every planned enemy
has materialized and every materialized slot passes the recovered death
predicate. The outer live gate then publishes:

```text
kTutorialEnemiesDefeated
```

which is the existing `tutorial_gameplay_state` transition `Attack -> Block`.

Focused smoke coverage verifies imported-health gating, exact Attack timing,
three ID `3` requests, state `0x2c` death suppression, stale-wave callback
rejection and one-shot completion.

## Remaining Android integration

The roster, spawn plan, imported HP resolver, scene-limit provider and tutorial
Attack runtime are all linked into the Android native target. The remaining
production boundary is to materialize reconstructed enemy actors for roster
slots and route their real combat damage/death/state changes back into the
roster with the active `wave_generation`.

Until that actor bridge exists, the runtime will not fabricate defeated enemies
or advance the tutorial from controls alone.
