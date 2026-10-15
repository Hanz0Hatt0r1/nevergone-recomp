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

## Reviewed inputs remain mandatory

`ReviewBundle` carries exactly the three semantic inputs that are not yet safe
to infer from the reconstructed live state:

1. `ReviewedPlayerFaceDir`;
2. `ReviewedSceneLimits`;
3. `game_levels_enemy_record_mapping::Mapping`.

The default live bundle contains no reviewed values. There is no JNI or
Developer Tools setter that can mark evidence reviewed, and no numeric fallback
is compiled into the Android path.

A candidate enemy-record mapping remains unusable. The base-health resolver is
provided to the Attack runtime only when
`game_levels_enemy_record_mapping::is_usable()` accepts a mapping whose review
status is `kReviewed` and whose identity/base-health fields are distinct.

## Live diagnostics

The existing `GameSceneAssetStagingDiagnostics.statusReport()` now appends a
`Tutorial Attack live gate` section. It reports:

- the current `tutorial_spawn_inputs` status;
- whether player face direction is reviewed;
- whether scene limits are reviewed;
- whether the enemy mapping is reviewed and usable;
- the current raw enemy-record count;
- the nested Attack-wave runtime status and wave generation;
- the number of successfully published completion edges.

This makes a device capture useful even while the gate is closed. For example,
`player-face-dir-unreviewed` is a concrete evidence dependency rather than a
silent failure to spawn enemies.

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

- the dynamic `PLAYERFACEDIR` contract;
- the tutorial scene-limit raw fields and vertical branch ordering;
- the enemy identity/base-health record mapping.

The existing Ghidra/runtime evidence pipelines should populate those providers
only after their artifacts are reviewed. Test-only constants in host smokes are
not production evidence and must not be copied into the live bundle.
