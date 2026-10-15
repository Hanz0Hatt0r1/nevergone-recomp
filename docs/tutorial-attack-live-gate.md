# Tutorial Attack live gate

`tutorial_attack_live_gate` is the Android/frame-level boundary between the
recovered first Attack-wave model and the live reconstructed scene/combat state.
It exercises the evidence-backed readiness chain without inventing values.

## Frame inputs

Every GL frame the live wrapper reads the current snapshots for:

- `tutorial_gameplay_state`;
- `game_scene_player_spawn_state`;
- `game_scene_player_motion_state`;
- `player_control_runtime_state`;
- `game_levels_runtime_state` and its current parsed PortNode;
- the retained raw `LoadGL_Global` enemy records;
- `game_clock::tick_count()`.

Those values are passed through `tutorial_spawn_inputs` and
`tutorial_attack_wave_runtime`. The gate does not maintain a second copy of
player or scene state.

## PLAYERFACEDIR is live-derived

The shipped ARMv7 binary proves that `MEPlayer::setPlayerDir(PLAYERFACEDIR)`,
`MEPlayer::isFace()`, and `MEPlayer::filpPlayer(bool, ...)` all use the same
`MEPlayer +0x12c` field. Together with the recovered horizontal control mapping:

```text
CONTROLSSTATE 0x12 -> flip_x=true  -> PLAYERFACEDIR 1
CONTROLSSTATE 0x13 -> flip_x=false -> PLAYERFACEDIR 0
```

`tutorial_player_face_dir_provider` derives a generation-stamped
`ReviewedPlayerFaceDir` every frame. An inconsistent enum/bool snapshot remains
unreviewed.

## Scene limits are live-derived

Direct bounded Thumb evidence in the exact shipped library and independent
`GameLevels::LoadGL_PortNode()` stores close the scene-limit mapping:

```text
third_u32  -> GameScenePortNodeData +0x4ac -> vertical_edge_a
fourth_u32 -> GameScenePortNodeData +0x4b0 -> vertical_edge_b
fifth_u32  -> GameScenePortNodeData +0x4b4 -> right_edge_x
```

`tutorial_scene_limit_provider` reads the current project-owned PortNode record,
requires its recovered scene-GUID string to match the runtime scene GUID, and
stamps the values with the current load-attempt count and GUID. A reload,
missing PortNode, or GUID mismatch fails closed.

## Enemy identity/base health are reviewed

Exact original consumers close the two `LoadGL_Global` fields required by the
first Attack roster:

```text
first_i32  -> GameLevelsEnemyData +0x14 -> enemy_id
second_i32 -> GameLevelsEnemyData +0x18 -> base_health
second_i32 -> GameLevelsEnemyData +0x1c -> total-blood mirror
```

The live wrapper installs
`game_levels_enemy_record_mapping::reviewed_original_mapping()` every frame via
the dedicated evidence setter. A candidate mapping remains unusable; the
production helper is reviewed from exact binary evidence rather than promoted
from runtime heuristics.

The resolver still requires one unique imported row for the requested enemy ID
and positive base health. Thus the Attack wave remains fail-closed if the loaded
scene data does not contain a valid ID `3` record.

## Live diagnostics

`GameSceneAssetStagingDiagnostics.statusReport()` appends:

- `Tutorial Attack live gate`;
- `Tutorial PLAYERFACEDIR provider`;
- `Tutorial scene-limit provider`.

The Attack section reports spawn-input status, whether the exact enemy mapping
is reviewed/usable, its identity/base-health field names, current raw enemy
record count, nested wave status/generation, and completion publication count.

With valid `cp_lesson.glData`, the expected state after Jump is now an active
project-owned Attack roster rather than `scene-limits-unreviewed` or
`waiting-for-base-health`.

## Enemy callback identity

Every damage/death/state callback must carry the active `wave_generation`
obtained from the Attack runtime. Reused roster slot indices are not stable
identities. Stale-generation callbacks are rejected, and a scene reload
invalidates the bound wave before later callbacks can mutate reused slots.

## Completion publication

The gate never advances the lesson from elapsed time or input alone. The roster
must first report all recovered Attack-wave enemies defeated under the recovered
`EnemyObject::isDead()` predicate. Only then does the live wrapper publish:

```text
TutorialEnemiesDefeated
```

to `tutorial_gameplay_state`. The runtime completion edge is consumed only
after that publication succeeds.

## Remaining production work

The first-wave geometry, identity, and imported HP inputs are now all
live/evidence-backed. The remaining blocker is the actual reconstructed enemy
actor bridge: materialize visible/interactive enemy entities for roster slots,
bind them to the current wave generation, and route real combat damage/death
state back into the roster. Test-only constants in host smokes are never
installed by the live path.
