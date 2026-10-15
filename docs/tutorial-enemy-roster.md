# Tutorial enemy roster

This slice connects the recovered `LessonLayer` enemy spawn plan to the recovered
`EnemyObject` health/death contract without inventing unresolved enemy data.

## Recovered inputs

`tutorial_enemy_spawn_plan` remains the authority for tutorial enemy IDs,
initial enemy states, positions and delays. For the first Attack lesson it plans
three enemy ID `3` requests in state `0x1c`: one immediate request, one delayed
by `1.0 s`, and one delayed by `0.5 s`.

The same recovered plan also uses enemy ID `3` for the Block and Execution
requests and enemy ID `0x15` (`21`) for the AirAttack request. These IDs are
independent evidence that can be compared with the raw `LoadGL_Global` enemy
records; they do not by themselves prove which record field is the identity
field.

`enemy_health_state` remains the authority for runtime HP and the recovered
`EnemyObject::isDead()` predicate. A slot is therefore not considered dead just
because its current health is non-positive when enemy state `0x0f`/`0x2c` or the
force-no-dead flag suppresses death.

## Imported-health boundary

The shipped `EnemyObject::loadEnemyObjectFile(GameLevelsEnemyData*, ...)` obtains
base HP from imported `GameLevelsEnemyData`. The clean-room `game_levels_model`
now retains the raw final `LoadGL_Global` enemy records, but original-consumer
evidence has not yet established which field is enemy identity or which field
is base health. `tutorial_enemy_roster` therefore still accepts a
`BaseHealthResolver` supplied by the future imported-data integration.

A due spawn whose resolver cannot provide a positive base-health value remains
pending. There is deliberately no fallback/default HP constant.

## Raw enemy-record evidence workflow

`game_levels_runtime_state::status_report()` emits at most the first 16 raw
records from the user-imported scene in physical parser order:

```text
i32, i32, f32, i32, i32, i32, i32, i32
```

The normal real-device GameScene evidence capture already stores those lines in
`artifacts/android-game-scene-runtime/runtime-diagnostics.txt`.

After a capture, export the records and compare integer columns against an
independently recovered tutorial enemy ID:

```bash
python3 tools/game_levels_enemy_record_evidence.py \
  artifacts/android-game-scene-runtime/runtime-diagnostics.txt \
  --known-enemy-id 3 \
  --output artifacts/android-game-scene-runtime/global-enemy-records.tsv
```

A second independently recovered ID can be added when it is known to be
relevant to the captured record set, for example the AirAttack ID:

```bash
python3 tools/game_levels_enemy_record_evidence.py \
  artifacts/android-game-scene-runtime/runtime-diagnostics.txt \
  --known-enemy-id 3 \
  --known-enemy-id 0x15 \
  --output artifacts/android-game-scene-runtime/global-enemy-records.tsv
```

The tool reports only **identity-field candidates**: integer columns whose
observed values contain every supplied known ID. It does not call such a column
proven, and it deliberately does not infer the HP field from value magnitude or
plausibility. When the bounded runtime report omitted records, candidate
coverage is explicitly marked incomplete.

`tools/game_levels_enemy_record_evidence_smoke.py` verifies contiguous indexing,
finite float parsing, total/omitted accounting, TSV export, truncated-report
handling, and candidate-column filtering.

## Cross-evidence mapping

The independent Ghidra consumer probe described in
`docs/enemy-health-death-contract.md` produces:

```text
build/enemy-health-field-consumer/enemy-health-field-consumer.tsv
```

Once both artifacts exist, combine them rather than interpreting either one by
itself:

```bash
python3 tools/enemy_record_cross_evidence.py \
  artifacts/android-game-scene-runtime/runtime-diagnostics.txt \
  build/enemy-health-field-consumer/enemy-health-field-consumer.tsv \
  --known-enemy-id 3 \
  --output artifacts/android-game-scene-runtime/enemy-record-cross-evidence.txt
```

When a second independently recovered ID is known to belong to the same captured
record set, add it as another `--known-enemy-id` (for example `0x15`). More than
one ID is useful because it can eliminate coincidental integer-column matches.

The cross-evidence analyzer emits a mapping candidate only when:

- the runtime report is complete rather than truncated;
- supplied known enemy IDs select exactly one integer identity column;
- the Ghidra consumer probe yields one shared source field for both recovered
  object health stores;
- the identity and base-health candidates are different raw fields.

The tool still does not rename parser fields automatically. Its output is the
bounded evidence package that should be reviewed before introducing semantic
`enemy_id` / `base_health` accessors and the production `BaseHealthResolver`.

## Wave-generation safety

Each roster mutation now requires the generation returned by
`Roster::snapshot().generation` after `begin()`. This applies to
`advance_to()`, damage, death, enemy-state changes, no-death overrides, and
`consume_all_defeated()`. Reused slot indices alone are not sufficient
identifiers: a delayed EnemyObject callback from an earlier wave must not
change or complete the next wave. `reset()` invalidates outstanding tokens
without resetting the generation counter.

The damage adapter rejects non-finite and negative amounts rather than
allowing a corrupted callback to poison the recovered health/death predicate.
This does not invent combat damage or enemy HP.

## Completion boundary

The roster exposes a one-shot `consume_all_defeated()` edge only when all
planned enemies have first materialized and every materialized slot passes the
recovered death predicate. The caller can then publish
`kTutorialEnemiesDefeated`, which is the existing `tutorial_gameplay_state`
transition from `Attack` to `Block`.

`tools/tutorial_enemy_roster_smoke.cpp` verifies:

- unresolved imported HP blocks materialization instead of manufacturing HP;
- Attack-wave timing at `0`, `0.5`, and `1.0 s`;
- three recovered enemy ID `3` requests and their recovered state/geometry;
- death suppression by state `0x2c`;
- one-shot wave completion;
- `kTutorialEnemiesDefeated` advances `Attack -> Block`.

## Android/runtime integration

`enemy_health_state.cpp`, `tutorial_enemy_spawn_plan.cpp`, and
`tutorial_enemy_roster.cpp` are linked into the Android native target. The
remaining production boundary is to prove and review the imported enemy
identity/base-HP mapping, provide that resolver to the roster, and connect live
`EnemyObject` damage/death updates to matching slots. Until that data path is
recovered, the roster will not guess HP or falsely advance the tutorial.
