# Player spawn runtime state

Issue #148 materializes the recovered type-6 player-entry contract as live project-owned runtime state. The retained point follows the complete `MEPlayer` position model and also preserves the original no-match behavior: the primary player already exists at its manager bootstrap point before `GameScene::loadingTex()` evaluates type-6 entry markers.

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

## No matching marker is a valid placement path

The type-6 branch in `GameScene::loadingTex()` is conditional. It compares the object's type tail against `GameLevels +0x40`; only a match executes the position transform and `MEPlayer::setPlayerPos()`. A non-match simply continues the object loop. There is no fatal branch that destroys the player or aborts GameScene construction.

The primary `MEPlayer` has already been created by `MEPlayerManager::initMEPlayerManager()` before this loop. Therefore a valid scene with zero matching type-6 markers retains the construction point unchanged:

```text
scene_offset = (0, 0)
player = bootstrap = (568, 448)
```

The runtime now reports this as `state: ready` with `matching markers: 0`, `applied markers: 0`, and `entry marker applied: no`. Malformed type-6 records, non-finite matching coordinates and invalid live surface dimensions still fail closed. When a valid matching marker exists, diagnostics report `entry marker applied: yes` and retain the recovered last-match overwrite semantics.

This distinction was required by the physical-device `run-335` evidence: static GameScene rendering and asset staging were already complete while the player motion state repeatedly reset because the reconstruction treated zero matching entry markers as a fatal spawn condition. The original does not.

## Android lifecycle

`PlayerActionRuntimeLoader.stageIfNeeded()` runs from the GL-thread GameScene asset staging path and refreshes player spawn state while the live scene queue is active. Physical surface dimensions remain part of the state identity so context/viewport changes trigger a refresh, but they no longer distort Cocos design-space coordinates.

Explicit imported-asset invalidation also resets this spawn state along with the parsed player action document.

The native runtime diagnostics now include the full `GameScene player spawn state` report so device captures distinguish bootstrap retention, a type-6 override, and malformed-marker failure without exposing proprietary resource names.

## Current #148 boundary

The project now owns both inputs required before constructing the first visible player node:

```text
live GameLevels scene + event-port type
    -> ordered matching type-6 markers
    -> if matched: design-space scene offset
    -> if unmatched: zero offset / retained bootstrap
    -> MEPlayer bootstrap + scene offset
    -> final visible player point

committed role Career
    -> pNN_actions.plist/action28/name
    -> parsed player .actData document
    -> MeActions current frame 0
    -> ActionFrameData +0x68 frame key
```

The earlier `EnemyActionsSystem +0x26c/+0x294` assumption does not apply to the player `MeActionsSystem`. Player playback uses `MeActionsSystem +0x1a4` for the current frame and `+0x1a8` for the previous frame; `initWithFile()`/`play()` prove the initial values `0/-1`.

The MeActions body-local transform is recovered in `docs/evidence/me-actions-initial-sprite-transform.md` and composed with the final player point in the project-owned render queue. The sprite appears when the selected imported action data and matching player atlas are available.
