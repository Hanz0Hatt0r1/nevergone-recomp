# Player spawn runtime state

Issue #148 materializes the recovered type-6 player-entry contract as live project-owned runtime state. The retained point now follows the complete `MEPlayer` position model rather than treating the type-6 transform as a final coordinate.

## Source state

The state is refreshed only after the offline GameLevels render queue is ready. The JNI bridge combines:

- `game_levels_runtime_state::current_scene_instance()`;
- the retained `stored_event_port_type`;
- the current GameScene renderer surface dimensions as a live-renderer readiness/revision key.

No scene-list `playerStartXPos/YPos` fallback is used because no value-flow edge has been proven from that metadata pair to the live type-6 `loadingTex()` position path.

## Design-space type-6 position

The original configures `setDesignResolutionSize(1136, 640, 0)` (Cocos2d-x ExactFit) before the relevant `CCDirector::getWinSize()` calls. The type-6 transform therefore operates in the recovered 1136x640 design coordinate space, not raw physical GLES pixel dimensions:

```text
scene_offset_x = source_x - 1136 * 0.5
scene_offset_y = source_y + 40.0 - 640 * 0.5
```

`MEPlayer::initWithMEName()` retains the construction/bootstrap point at `MEPlayer +0x240`. For the primary offline player, `MEPlayerManager::initMEPlayerManager()` constructs that point as:

```text
bootstrap_x = 1136 * 0.5 = 568
bootstrap_y = 640 * 0.7 = 448
```

`MEPlayer::setPlayerPos()` stores the transformed type-6 point at `MEPlayer +0x248` and sets the active action node to the sum. `MEPlayer::getPlayerPos()` independently returns the same sum:

```text
player_x = bootstrap_x + scene_offset_x
player_y = bootstrap_y + scene_offset_y
```

Thus at this design resolution the final visible first-scene point simplifies to:

```text
player_x = source_x
player_y = source_y + 168
```

The original `GameScene::loadingTex()` calls `MEPlayer::setPlayerPos()` for every matching type-6 marker rather than enforcing uniqueness. The project preserves that ordered overwrite behavior: all matches are counted and the final live position comes from the last matching marker.

Missing markers, malformed matching records, non-finite coordinates and invalid live surface dimensions fail closed and clear the retained position.

## Android lifecycle

`PlayerActionRuntimeLoader.stageIfNeeded()` runs from the GL-thread GameScene asset staging path and refreshes player spawn state while the live scene queue is active. Physical surface dimensions remain part of the state identity so context/viewport changes trigger a refresh, but they no longer distort Cocos design-space coordinates.

Explicit imported-asset invalidation also resets this spawn state along with the parsed player action document.

## Current #148 boundary

The project now owns both inputs required before constructing the first visible player node:

```text
live GameLevels scene + event-port type
    -> ordered matching type-6 markers
    -> design-space scene offset
    -> MEPlayer bootstrap + scene offset
    -> final visible player point

committed role Career
    -> pNN_actions.plist/action28/name
    -> parsed player .actData document
    -> MeActions current frame 0
    -> ActionFrameData +0x68 frame key
```

The earlier `EnemyActionsSystem +0x26c/+0x294` assumption does not apply to the player `MeActionsSystem`. Player playback uses `MeActionsSystem +0x1a4` for the current frame and `+0x1a8` for the previous frame; `initWithFile()`/`play()` prove the initial values `0/-1`.

The MeActions body-local transform is now recovered in `docs/evidence/me-actions-initial-sprite-transform.md` and composed with the final player point in the project-owned render queue. The sprite appears when the selected imported action data and matching player atlas are available.
