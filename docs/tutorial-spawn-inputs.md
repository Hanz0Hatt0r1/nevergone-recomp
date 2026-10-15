# Tutorial spawn input readiness gate

`tutorial_enemy_spawn_plan` already contains the recovered enemy geometry, but
starting a live wave requires a coherent snapshot of player and scene state.
`tutorial_spawn_inputs` is the fail-closed boundary that assembles that snapshot
without inventing unresolved combat data.

## Reconstructed inputs

The adapter consumes the existing live snapshots for:

- `game_scene_player_spawn_state`: base player position and scene generation;
- `game_scene_player_motion_state`: accumulated evidence-backed movement offset;
- `player_control_runtime_state`: current horizontal control generation and the
  `flip_x` bool retained from the same value passed to `MEPlayer::filpPlayer()`;
- `game_levels_runtime_state`: current loaded scene/load-attempt identity and
  current parsed PortNode record.

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

## Scene limits are now live-derived

Direct ARMv7 evidence in `LessonLayer::nextLesson()` closes the raw scene-limit
field mapping and comparison order:

```text
PortNode third_u32  -> owner +0x4ac -> vertical_edge_a
PortNode fourth_u32 -> owner +0x4b0 -> vertical_edge_b
PortNode fifth_u32  -> owner +0x4b4 -> right_edge_x
```

`GameLevels::LoadGL_PortNode()` independently proves that these three serialized
uint32 values are stored into those exact `GameScenePortNodeData` fields. The
clean-room parser already retains the same record, and runtime navigation
already exposes the current PortNode.

`tutorial_scene_limit_provider` therefore creates `ReviewedSceneLimits` directly
from the current parsed PortNode. It requires the PortNode's recovered first
scene-GUID string to equal the current runtime scene GUID, then stamps the values
with the exact runtime load-attempt count and scene GUID. A reload or navigation
mismatch fails closed.

## Readiness checks

`tutorial_spawn_inputs::resolve()` returns `kReady` only when all of these are
true:

- player spawn state is ready;
- current GameLevels runtime and render queue are ready;
- spawn and runtime refer to the same non-empty scene GUID;
- motion state is ready and belongs to the exact spawn generation;
- horizontal facing has been established;
- the evidence-backed `PLAYERFACEDIR` belongs to the exact control generation;
- PortNode-derived scene limits belong to the exact runtime load attempt/scene;
- the resulting player coordinates are finite.

There is no default face direction, no default map extent, and no reuse of
reviewed values across generations.

## Remaining Attack-wave gate

The geometry side of the first live Attack wave is now evidence-backed. The
remaining materialization sequence is:

1. derive current `ReviewedPlayerFaceDir` from live controls;
2. derive current `ReviewedSceneLimits` from the selected parsed PortNode;
3. call `tutorial_spawn_inputs::resolve()`;
4. resolve enemy ID/base HP through the separately reviewed
   `game_levels_enemy_record_mapping` boundary;
5. materialize the recovered Attack roster only when both geometry and imported
   enemy data are ready;
6. publish the tutorial zero-live-enemy event only after the roster's recovered
   death predicate says every materialized enemy is dead.

The remaining production evidence blocker is therefore the enemy identity/base-
health record mapping, not tutorial map geometry.
