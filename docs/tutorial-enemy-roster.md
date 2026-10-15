# Tutorial enemy roster

This slice connects the recovered `LessonLayer` enemy spawn plan to the recovered
`EnemyObject` health/death contract without inventing unresolved enemy data.

## Recovered inputs

`tutorial_enemy_spawn_plan` remains the authority for tutorial enemy IDs,
initial enemy states, positions and delays. For the first Attack lesson it plans
three enemy ID `3` requests in state `0x1c`: one immediate request, one delayed
by `1.0 s`, and one delayed by `0.5 s`.

`enemy_health_state` remains the authority for runtime HP and the recovered
`EnemyObject::isDead()` predicate. A slot is therefore not considered dead just
because its current health is non-positive when enemy state `0x0f`/`0x2c` or the
force-no-dead flag suppresses death.

## Imported-health boundary

The shipped `EnemyObject::loadEnemyObjectFile(GameLevelsEnemyData*, ...)` obtains
base HP from imported `GameLevelsEnemyData`. The clean-room `game_levels_model`
does not yet expose that enemy-data record, so `tutorial_enemy_roster` accepts a
`BaseHealthResolver` supplied by the future imported-data integration.

A due spawn whose resolver cannot provide a positive base-health value remains
pending. There is deliberately no fallback/default HP constant.

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
`tutorial_enemy_roster.cpp` are linked into the Android native target by this
slice. The remaining production boundary is to expose imported
`GameLevelsEnemyData` health and connect live `EnemyObject` damage/death updates
to the matching roster slots. Until that data path is recovered, the roster
will not guess HP or falsely advance the tutorial.
