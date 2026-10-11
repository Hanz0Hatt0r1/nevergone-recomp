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

`player_control_contract::horizontal_primary_state()` models only the proven horizontal assignment. It retains the native values as `state-0x12` and `state-0x13`; later evidence below now proves their sprite-facing effect without assigning a world-space velocity to either state.

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

`MEPlayer::setControlsState()` stores the accepted primary Controls state at `MEPlayer +0x1f0`. A bounded disassembly of the exact imported ARMv7 library now establishes the facing branch precisely:

- near ELF `0x0031de36`, primary state `0x12` checks the retained facing byte at `MEPlayer +0x12c` and, when a change is required, calls `MEPlayer::filpPlayer(true, ...)`;
- near ELF `0x0031de50`, primary state `0x13` checks the same byte and calls `MEPlayer::filpPlayer(false, ...)` when a change is required;
- the same `0x12 -> true` / `0x13 -> false` mapping repeats on the later state path near `0x0031df3e` / `0x0031df4c`.

`MEPlayer::filpPlayer(bool, MEPLAYERSTATE)` starts at ELF `0x0031a4ec`. It retains the boolean at `MEPlayer +0x12c` and forwards it to `MeActionsSystem::filpAction(bool)` at ELF `0x00329108`. That action-system function stores the flag at `MeActionsSystem +0x1be` and calls `CCSprite::setFlipX(bool)` for the primary body sprite at `MeActionsSystem +0x118`.

The project-owned mapping is therefore evidence-backed and intentionally narrow:

```text
primary Controls state 0x12 -> body sprite flipX = true
primary Controls state 0x13 -> body sprite flipX = false
```

`player_control_contract::facing_flip_x()` models exactly this mapping and rejects any other enum value. `player_control_runtime_state` retains only the currently proven primary horizontal state and its resulting flip flag. `game_scene_player_render_queue` applies that flag to the live reconstructed player body transform, so a later real input transport can produce a visible state change without inventing position deltas.

The same bounded `MEPlayer::setControlsState()` path can also move the current action into recovered `MEPLAYERSTATE` values such as `0x10`, `0x11`, or `0x17` depending on surrounding state. Returning to primary Controls state `0` can transition to recovered action state `0x1b`. Those action-state numbers remain evidence only; animation names are not assigned until the corresponding EnemyActions action mapping is proven.

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

There are multiple call sites to `MEPlayer::movePlayer(CCPoint)` in scene update/collision/camera logic, so treating joystick power as direct world-space velocity would collapse distinct shipped responsibilities. The reconstructed input layer therefore continues to stop at the proven `CONTROLSSTATE`/facing boundary until a movement delta is recovered independently.

## Current #148 boundary

The project now has evidence-backed pieces for:

- selected Career -> offline player index;
- initial `MEPlayerManager` bootstrap point;
- first-scene event-port marker -> exact offline entry-position transform;
- first EnemyActions frame lookup -> project-owned sprite-frame request;
- initial MeActions body transform -> project-owned player sprite geometry;
- player-frame resolution restricted to the selected Career's shipped default plists;
- player sprite insertion into the first live GameScene render queue at the recovered spawn point;
- joystick horizontal classification -> primary Controls states `0x12` / `0x13`;
- exact `GameScene::tick()` delivery order into the primary `MEPlayer`;
- exact primary-state -> `CCSprite::setFlipX` mapping;
- a project-owned runtime state that applies the recovered facing flag to the live player render queue.

Still required before P8/P9 can be called complete:

- recover or otherwise evidence the live first-scene joystick touch/input transport so Android events produce the same activity/angle sample boundary instead of using an invented screen-region mapping;
- on-device evidence with user-owned imported data that changing joystick direction visibly flips the reconstructed player without crashing or bypassing the recovered scene constraints;
- recover the separate position/movement boundary only if a positional change is required beyond the now-proven visible facing-state change.

The next implementation step is the original joystick input surface/event route. Movement speed remains deliberately out of scope until native evidence identifies it.
