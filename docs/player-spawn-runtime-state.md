# Player spawn runtime state

Issue #148 now materializes the already recovered type-6 player-entry contract as live project-owned runtime state.

## Source state

The state is refreshed only after the offline GameLevels render queue is ready. The JNI bridge combines:

- `game_levels_runtime_state::current_scene_instance()`;
- the retained `stored_event_port_type`;
- the current GameScene renderer surface dimensions.

No scene-list `playerStartXPos/YPos` fallback is used because no value-flow edge has been proven from that metadata pair to the live `GameScene + 0x4c4` cursor.

## Ordered type-6 application

`game_scene_player_spawn_state::refresh()` delegates marker selection to the existing evidence-backed `game_scene_player_entry_markers::collect()` contract and applies the exact offline transform to every matching marker in source traversal order:

```text
player_x = source_x - surface_width  * 0.5
player_y = source_y + 40.0 - surface_height * 0.5
```

The original `GameScene::loadingTex()` calls `MEPlayer::setPlayerPos()` for every matching type-6 marker rather than enforcing uniqueness. The project-owned state preserves that behavior: every valid marker is counted/applied and the final observable position is the last transformed match.

Missing markers, malformed matching records, non-finite coordinates and invalid surface dimensions fail closed and clear the retained position.

## Android lifecycle

`PlayerActionRuntimeLoader.stageIfNeeded()` already runs from the GL-thread GameScene asset staging path. It now refreshes the player spawn state while the live scene queue is active. The native bridge reads surface dimensions from `game_scene_direct_renderer::snapshot()`, which is synchronized from the current GLES viewport before the static GameScene draw.

Explicit imported-asset invalidation also resets this spawn state along with the parsed player action document.

## Current #148 boundary

The project now owns both inputs required before constructing a visible player node:

```text
live GameLevels scene + event-port type + surface size
    -> ordered matching type-6 markers
    -> exact transformed player position

committed role Career
    -> pNN_actions.plist/action28/name
    -> parsed player .actData document
```

The initial player frame is now traced through the separate `MeActionsSystem` path: `initWithFile()` and `play()` seed `+0x1a4` to zero before the first `updateData()` lookup. See [`evidence/player-initial-meactions-frame.md`](evidence/player-initial-meactions-frame.md). The `EnemyActionsSystem +0x294` state and `+0x26c` readiness byte do not apply to this player object.

Once that initial current-frame transition is proven, the retained document and spawn point can be passed directly through `game_scene_player_frame_request` and attached to a project-owned renderer node without guessed coordinates or frame selection.
