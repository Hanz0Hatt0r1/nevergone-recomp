# Tutorial spawn input readiness gate

`tutorial_enemy_spawn_plan` already contains the recovered enemy geometry, but
starting a live wave requires a coherent snapshot of player and scene state.
`tutorial_spawn_inputs` is the fail-closed boundary that assembles that snapshot
without inventing still-unresolved scene data.

## Already reconstructed inputs

The adapter consumes the existing live snapshots for:

- `game_scene_player_spawn_state`: base player position and scene generation;
- `game_scene_player_motion_state`: accumulated evidence-backed movement offset;
- `player_control_runtime_state`: current horizontal control generation and the
  `flip_x` bool retained from the same value passed to `MEPlayer::filpPlayer()`;
- `game_levels_runtime_state`: current loaded scene/load-attempt identity.

The current tutorial position is therefore:

```text
x = spawn.player_x + motion.offset_x
y = spawn.player_y + motion.offset_y
```

`player_is_face` is sourced from the recovered `flip_x` path. A bounded ARMv7
review also proves that `MEPlayer::setPlayerDir(PLAYERFACEDIR)`, `isFace()`, and
`filpPlayer(bool, ...)` all use the same `MEPlayer +0x12c` field. The live
`tutorial_player_face_dir_provider` therefore produces:

```text
CONTROLSSTATE 0x12 -> flip_x=true  -> PLAYERFACEDIR 1
CONTROLSSTATE 0x13 -> flip_x=false -> PLAYERFACEDIR 0
```

stamped with the exact `player_control_runtime_state` generation. The provider
fails closed if the retained enum and bool disagree.

## Input that remains evidence-gated

`ReviewedSceneLimits` still requires the two vertical edges and right-side X
value read through the recovered `GameScene +0x238 -> +0x20` path. The reviewed
set is stamped with both the exact `GameLevels` load-attempt count and current
scene GUID; a scene reload makes it stale and blocks wave creation.

The separate enemy identity/base-health mapping remains a later materialization
gate rather than a spawn-geometry input.

## Readiness checks

`tutorial_spawn_inputs::resolve()` returns `kReady` only when all of these are
true:

- player spawn state is ready;
- current GameLevels runtime and render queue are ready;
- spawn and runtime refer to the same non-empty scene GUID;
- motion state is ready and belongs to the exact spawn generation;
- horizontal facing has been established;
- the evidence-backed `PLAYERFACEDIR` belongs to the exact control generation;
- scene limits are reviewed and belong to the exact runtime load attempt/scene;
- the resulting player coordinates are finite.

There is no default face direction, no default map extent, and no reuse of
reviewed values across generations.

## Integration sequence

The remaining live Attack-wave integration is now:

1. derive current `ReviewedPlayerFaceDir` through the recovered shared
   `MEPlayer +0x12c` facing contract;
2. run/review the `tutorial-scene-limit-evidence` pipeline and recover the three
   raw scene-limit fields plus vertical A/B branch ordering;
3. generate `ReviewedSceneLimits` for the current scene/load generation;
4. call `tutorial_spawn_inputs::resolve()`;
5. only when it returns `kReady`, pass `Result::inputs` to the existing Attack
   wave runtime/roster;
6. resolve enemy ID/base HP through the separately reviewed
   `game_levels_enemy_record_mapping` boundary before materialization;
7. publish the tutorial zero-live-enemy event only after the roster's recovered
   death predicate says every materialized enemy is dead.

This keeps the first combat tutorial blocked only on concrete original-game
evidence that is actually still missing.
