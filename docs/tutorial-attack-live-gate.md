# Tutorial Attack live gate

`tutorial_attack_live_gate` is the Android/frame-level boundary between the
recovered first Attack-wave model and evidence that is still pending review. It
exercises and reports the real readiness chain without inventing values for
unresolved original-game fields.

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
`GameLevels::LoadGL_PortNode()` stores close the previous scene-limit gap:

```text
third_u32  -> GameScenePortNodeData +0x4ac -> vertical_edge_a
fourth_u32 -> GameScenePortNodeData +0x4b0 -> vertical_edge_b
fifth_u32  -> GameScenePortNodeData +0x4b4 -> right_edge_x
```

`tutorial_scene_limit_provider` reads the current project-owned PortNode record,
requires its recovered scene-GUID string to match the runtime scene GUID, and
stamps the values with the current load-attempt count and GUID. No numeric
fallback or developer setter is involved. A reload, missing PortNode, or GUID
mismatch makes the provider unreviewed.

## Remaining mandatory evidence

The remaining pre-materialization evidence gate is
`game_levels_enemy_record_mapping::Mapping`.

A candidate enemy-record mapping remains unusable. The base-health resolver is
provided to the Attack runtime only when
`game_levels_enemy_record_mapping::is_usable()` accepts a mapping whose review
status is `kReviewed` and whose identity/base-health fields are distinct.

## Live diagnostics

`GameSceneAssetStagingDiagnostics.statusReport()` appends:

- `Tutorial Attack live gate`;
- `Tutorial PLAYERFACEDIR provider`;
- `Tutorial scene-limit provider`.

The report includes spawn-input status, provider readiness/current generations,
resolved scene-limit values when reviewed, enemy mapping readiness, current raw
enemy-record count, nested Attack-wave status/wave generation, and completion
publication count.

Once facing is established and the current PortNode resolves, the expected next
production blocker is `waiting-for-base-health` until enemy record mapping is
reviewed.

## Enemy callback identity

Once materialization is possible, every damage/death/state callback must carry
the active `wave_generation` obtained from the Attack runtime. Reused roster
slot indices are not stable identities. Callbacks carrying a stale generation
are rejected by both the live gate and underlying roster/runtime contracts.

A scene reload also invalidates the bound wave before later callbacks can mutate
reused slots.

## Completion publication

The gate never advances the lesson from elapsed time or input alone. The roster
must first report all recovered Attack-wave enemies defeated under the recovered
`EnemyObject::isDead()` predicate. Only then does the live wrapper publish:

```text
TutorialEnemiesDefeated
```

to `tutorial_gameplay_state`. The runtime completion edge is consumed only
after that publication succeeds.

## Remaining evidence work

The first-wave geometry inputs are now live and evidence-backed. The remaining
production reconstruction is the enemy identity/base-health mapping and then the
actual EnemyObject materialization/damage callback bridge. Test-only constants
in host smokes are not production evidence and are never installed by the live
path.
