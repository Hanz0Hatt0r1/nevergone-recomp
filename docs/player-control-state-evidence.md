# Player control-state evidence

This note narrows issue #148 to the first evidence-backed input boundary. The observations are derived from the user-owned Nevergone 1.0.9 ARMv7 `libcocos2dcpp.so` (SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`). The original binary and assets remain outside Git.

The purpose of this boundary is deliberately smaller than “implement movement”: recover how the shipped joystick updates the control state consumed by the primary `MEPlayer`, without inventing movement speed, acceleration, animation timing or combat transitions.

## Joystick callback

The shipped export

```text
Controls::onCCJoyStickUpdate(CCNode*, float, CCPoint, float)
```

starts at ELF `0x002973dc` (Thumb symbol value `0x002973dd`). The bounded disassembly establishes the following behavior:

- the callback exits when the Controls input-enabled byte at `+0x170` is false;
- it verifies the callback node against the retained joystick pointer at `+0x124`;
- it copies the callback `CCPoint` into Controls state at `+0x168`;
- it compares the activity value already retained at Controls `+0x160` against `0.05f`;
- above that threshold, the callback classifies the angle value against `-180`, `-90`, and `90` degrees;
- angles from `-90` through `90` inclusive write primary control state `0x13` to Controls `+0x10c`;
- angles below `-90` down through `-180`, and angles above `90`, write primary control state `0x12`;
- values below `-180` do not reach either recovered state assignment;
- after the horizontal assignment path, the callback calls `Controls::getDirectionID(CCPoint, 0.5f, 0.01f)` and then `Controls::comboJoyStickUpdate(...)`.

`player_control_contract::horizontal_primary_state()` models only the proven horizontal assignment. It uses the neutral names `state-0x12` and `state-0x13`: the current bounded evidence is enough to prove two horizontal hemispheres, but this reconstruction does not need to guess a user-facing left/right name to progress.

The model also rejects non-finite inputs. The original runtime normally receives finite joystick values; fail-closed behavior is preferable for reconstructed state supplied from project-owned code.

## Per-frame delivery into MEPlayer

`GameScene::tick(float)` starts at ELF `0x003475a8` (Thumb symbol value `0x003475a9`). In the recovered input block it obtains the current `Controls` object from `GameSceneUI` and dispatches four state fields in this order:

1. Controls `+0x110` -> `GameScene::setDirComboState()`;
2. Controls `+0x11c` -> `GameScene::setControlsDirectionState()`;
3. Controls `+0x118` -> `GameScene::setFunctionState()`;
4. Controls `+0x10c` -> `GameScene::setControlsState()`.

The same tick then calls `MEPlayerManager::tick(float)`.

`GameScene::setControlsState(CONTROLSSTATE)` at ELF `0x0033ff7e` obtains the shared `MEPlayerManager`, loads its primary player slot at `+0x104`, and tail-calls `MEPlayer::setControlsState(CONTROLSSTATE)` at ELF `0x0031ddee`.

`GameScene::setControlsDirectionState(CONTROLSSTATE)` at ELF `0x0033ffae` likewise obtains the primary player and stores the state at `MEPlayer +0x1fc`.

`player_control_contract::tick_dispatches()` retains the proven four-way order so later integration can be regression-tested without hard-coding it ad hoc in rendering/input glue.

## What states 0x12 / 0x13 do in MEPlayer

`MEPlayer::setControlsState()` stores the accepted primary Controls state at `MEPlayer +0x1f0`. The bounded state path treats `0x12` and `0x13` as a pair:

- they participate in the facing/flip branch through `MEPlayer::filpPlayer(...)`;
- they can move the current action into recovered `MEPLAYERSTATE` values such as `0x10`, `0x11`, or `0x17` depending on the surrounding current player/action state;
- returning to primary Controls state `0` can transition the player into recovered action state `0x1b` on the observed path.

Those action-state numbers are retained here as evidence only. Their animation names are not assigned until the corresponding EnemyActions frame/action mapping is proven for the first offline player.

## Position remains a separate boundary

The input contract intentionally does not calculate a position delta.

Existing evidence already proves separate position APIs:

```text
MEPlayer::setPlayerPos(CCPoint)
MEPlayer::getPlayerPos()
MEPlayer::movePlayer(CCPoint)
GameScene::setPlayerNewPos(CCPoint)
GameScene::setPlayerPosAddOffset(CCPoint)
```

and the first-scene type-6 entry marker path is modeled by `game_scene_player_entry_markers`.

There are multiple call sites to `MEPlayer::movePlayer(CCPoint)` in scene update/collision/camera logic, so treating joystick power as direct world-space velocity would collapse distinct shipped responsibilities. The reconstructed input layer therefore stops at the `CONTROLSSTATE` boundary.

## Current #148 boundary

The project now has evidence-backed pieces for:

- selected Career -> offline player index;
- initial `MEPlayerManager` bootstrap point;
- first-scene event-port marker -> exact offline entry-position transform;
- EnemyActions frame lookup -> project-owned sprite-frame request;
- joystick horizontal classification -> primary Controls states `0x12` / `0x13`;
- exact `GameScene::tick()` delivery order into the primary `MEPlayer`.

Still required before P8/P9 can be called complete:

- a proven first visible player frame/resource selector from user-owned imported data;
- integration of that frame into the existing static/dynamic GameScene render path at the recovered spawn point;
- a live input bridge that feeds the project-owned Controls state into that player instance;
- on-device evidence that a joystick direction produces a visible player-state/position change without crashing or bypassing the recovered scene constraints.

The next implementation step should connect the already-recovered first-player frame request and spawn marker to a renderer/player instance. It should not invent movement constants merely to make the avatar move.
