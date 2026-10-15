# Tutorial Attack live gate

`tutorial_attack_live_gate` is the Android/frame-level boundary between the
already recovered first Attack-wave model and evidence that is still pending
review. It exists so the GL loop can exercise and report the real readiness
chain without inventing values for unresolved original-game fields.

## Frame inputs

Every GL frame the live wrapper reads the current snapshots for:

- `tutorial_gameplay_state`;
- `game_scene_player_spawn_state`;
- `game_scene_player_motion_state`;
- `player_control_runtime_state`;
- `game_levels_runtime_state`;
- the retained raw `LoadGL_Global` enemy records;
- `game_clock::tick_count()`.

Those values are passed through the existing `tutorial_spawn_inputs` and
`tutorial_attack_wave_runtime` contracts. The gate does not maintain a second
copy of player or scene state.

## PLAYERFACEDIR is now live-derived

The shipped ARMv7 binary proves that `MEPlayer::setPlayerDir(PLAYERFACEDIR)`,
`MEPlayer::isFace()`, and `MEPlayer::filpPlayer(bool, ...)` all use the same
`MEPlayer +0x12c` field. Together with the already recovered horizontal control
mapping this yields the exact bounded contract:

```text
CONTROLSSTATE 0x12 -> flip_x=true  -> PLAYERFACEDIR 1
CONTROLSSTATE 0x13 -> flip_x=false -> PLAYERFACEDIR 0
```

`tutorial_player_face_dir_provider` derives a generation-stamped
`ReviewedPlayerFaceDir` from the live control snapshot each frame. It accepts
the value only when the primary state and retained `flip_x` agree with the
recovered mapping. An inconsistent snapshot remains unreviewed.

## Inputs that remain mandatory

The live gate still requires independent reviewed evidence for:

1. `ReviewedSceneLimits`;
2. `game_levels_enemy_record_mapping::Mapping`.

There is no JNI or Developer Tools setter that can silently mark either value
reviewed, and no numeric fallback is compiled into the Android path.

A candidate enemy-record mapping remains unusable. The base-health resolver is
provided to the Attack runtime only when
`game_levels_enemy_record_mapping::is_usable()` accepts a mapping whose review
status is `kReviewed` and whose identity/base-health fields are distinct.

## Live diagnostics

The existing `GameSceneAssetStagingDiagnostics.statusReport()` appends both a
`Tutorial Attack live gate` section and `Tutorial PLAYERFACEDIR provider`
status. It reports:

- the current `tutorial_spawn_inputs` status;
- whether current control state produced reviewed PLAYERFACEDIR;
- the control generation and resolved integer value when available;
- whether scene limits are reviewed;
- whether the enemy mapping is reviewed and usable;
- the current raw enemy-record count;
- the nested Attack-wave runtime status and wave generation;
- the number of successfully published completion edges.

After horizontal facing is established, the expected next production blocker is
therefore `scene-limits-unreviewed`, not `player-face-dir-unreviewed`.

## Enemy callback identity

Once materialization is possible, every damage/death/state callback must carry
the active `wave_generation` obtained from the Attack runtime. Reused roster
slot indices are not stable identities. Callbacks carrying a stale generation
are rejected by both the live gate and the underlying roster/runtime contracts.

## Completion publication

The gate never advances the lesson from elapsed time or input alone. The
underlying roster must first report all recovered Attack-wave enemies defeated
under the recovered `EnemyObject::isDead()` predicate. Only then does the live
wrapper publish:

```text
TutorialEnemiesDefeated
```

to `tutorial_gameplay_state`. The runtime completion edge is consumed only
after that publication succeeds, preventing the wave from being discarded if
the outer state machine rejects the event.

## Remaining evidence work

The live path intentionally remains blocked until the repository gains reviewed
providers for:

- the tutorial scene-limit raw fields and vertical branch ordering;
- the enemy identity/base-health record mapping.

Test-only constants in host smokes are not production evidence and must not be
copied into either provider.
