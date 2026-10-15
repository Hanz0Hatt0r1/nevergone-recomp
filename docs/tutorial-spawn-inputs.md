# Tutorial spawn input readiness gate

`tutorial_enemy_spawn_plan` already contains the recovered enemy geometry, but
starting a live wave requires a coherent snapshot of player and scene state.
`tutorial_spawn_inputs` is the fail-closed boundary that assembles that snapshot
without inventing the still-unresolved fields.

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

and `player_is_face` is sourced from the recovered `flip_x` path. The adapter
requires an established horizontal primary state before using that bool.

## Inputs that remain evidence-gated

Two inputs are deliberately not derived from guesses:

1. `ReviewedPlayerFaceDir` — the integer `PLAYERFACEDIR` forwarded by the
   original `LessonLayer` into enemy creation;
2. `ReviewedSceneLimits` — the two vertical edges and right-side X value read
   through the recovered `GameScene +0x238 -> +0x20` path.

A reviewed player-face-dir value is stamped with the exact live control
generation. A reviewed scene-limit set is stamped with both the exact
`GameLevels` load-attempt count and current scene GUID. Any later input change or
scene reload makes the corresponding value stale and blocks wave creation.

## Readiness checks

`tutorial_spawn_inputs::resolve()` returns `kReady` only when all of these are
true:

- player spawn state is ready;
- current GameLevels runtime and render queue are ready;
- spawn and runtime refer to the same non-empty scene GUID;
- motion state is ready and belongs to the exact spawn generation;
- horizontal facing has been established;
- `PLAYERFACEDIR` is reviewed and belongs to the exact control generation;
- scene limits are reviewed and belong to the exact runtime load attempt/scene;
- the resulting player coordinates are finite.

There is no fallback face direction, no default map extent, and no reuse of
reviewed values across generations.

## Integration sequence

The remaining live Attack-wave integration is now explicit:

1. run/review the `tutorial-player-face-field-evidence` pipeline and recover the
   dynamic `PLAYERFACEDIR` value contract;
2. run/review the `tutorial-scene-limit-evidence` pipeline and recover the three
   raw scene-limit fields plus vertical A/B branch ordering;
3. generate `ReviewedPlayerFaceDir` and `ReviewedSceneLimits` for the current
   live generations;
4. call `tutorial_spawn_inputs::resolve()`;
5. only when it returns `kReady`, pass `Result::inputs` to
   `tutorial_enemy_roster::begin(kAttackWave, ...)`;
6. resolve enemy ID/base HP through the separately reviewed
   `game_levels_enemy_record_mapping` boundary before materialization;
7. publish the tutorial zero-live-enemy event only after the roster's recovered
   death predicate says every materialized enemy is dead.

This keeps the first combat tutorial blocked on concrete original-game evidence
rather than replacing unknown values with visually plausible constants.
