# Player control-state evidence

This note records the evidence-backed input boundary for issue #148. The observations are derived from the user-owned Nevergone 1.0.9 ARMv7 `libcocos2dcpp.so` (SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`) plus the later physical-device runtime trace documented in `evidence/original-runtime-player-motion-2026-10-12.md`. The original binary, runtime trace and assets remain outside Git.

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

The physical-device trace later confirms that `Controls +0x168/+0x16c` is the normalized joystick direction vector, while `+0x160` remains the separate activity/power-like value. Negative normalized X accompanies state `0x12` and positive normalized X accompanies state `0x13` in the accepted runtime samples.

`player_control_contract::horizontal_primary_state()` retains only the proven horizontal state assignment and rejects non-finite inputs.

## Per-frame delivery into MEPlayer

`GameScene::tick(float)` starts at ELF `0x003475a8` (Thumb symbol value `0x003475a9`). In the recovered input block it obtains the current `Controls` object from `GameSceneUI` and dispatches four state fields in this order:

1. Controls `+0x110` -> `GameScene::setDirComboState()`;
2. Controls `+0x11c` -> `GameScene::setControlsDirectionState()`;
3. Controls `+0x118` -> `GameScene::setFunctionState()`;
4. Controls `+0x10c` -> `GameScene::setControlsState()`.

The same tick then calls `MEPlayerManager::tick(float)`.

`GameScene::setControlsState(CONTROLSSTATE)` at ELF `0x0033ff7e` obtains the shared `MEPlayerManager`, loads its primary player slot at `+0x104`, and tail-calls `MEPlayer::setControlsState(CONTROLSSTATE)` at ELF `0x0031ddee`.

`GameScene::setControlsDirectionState(CONTROLSSTATE)` at ELF `0x0033ffae` likewise obtains the primary player and stores the state at `MEPlayer +0x1fc`.

`player_control_contract::tick_dispatches()` retains the proven four-way order.

## What states 0x12 / 0x13 do in MEPlayer

`MEPlayer::setControlsState()` stores the accepted primary Controls state at `MEPlayer +0x1f0`. A bounded disassembly of the exact imported ARMv7 library establishes the facing branch precisely:

- near ELF `0x0031de36`, primary state `0x12` checks the retained facing byte at `MEPlayer +0x12c` and, when a change is required, calls `MEPlayer::filpPlayer(true, ...)`;
- near ELF `0x0031de50`, primary state `0x13` checks the same byte and calls `MEPlayer::filpPlayer(false, ...)` when a change is required;
- the same `0x12 -> true` / `0x13 -> false` mapping repeats on the later state path near `0x0031df3e` / `0x0031df4c`.

`MEPlayer::filpPlayer(bool, MEPLAYERSTATE)` at ELF `0x0031a4ec` retains the boolean at `MEPlayer +0x12c` and forwards it to `MeActionsSystem::filpAction(bool)` at ELF `0x00329108`. That function stores the flag at `MeActionsSystem +0x1be` and calls `CCSprite::setFlipX(bool)` for the primary body sprite at `MeActionsSystem +0x118`.

The project-owned mapping is therefore:

```text
primary Controls state 0x12 -> body sprite flipX = true
primary Controls state 0x13 -> body sprite flipX = false
```

The same bounded `MEPlayer::setControlsState()` path establishes the minimum normal locomotion transition used by the reconstruction: from idle action state `0x1b`, horizontal state `0x12` or `0x13` can request action state `0`. Returning to neutral can return the normal path to `0x1b`, subject to the surrounding native state gates.

## Normal and fast position movement are now recovered

The accepted original-device runtime trace and a bounded disassembly of `MEPlayer::playerMoveValue(CCPoint)` at ELF `0x0031d9c0` identify the movement delta independently of joystick power.

For action state `0` and player indices `1..3`:

```text
movement.x = normalized_direction.x * 8.0
movement.y = normalized_direction.y * 4.0
```

For action state `1`:

```text
player index 1 -> x*20, y*10
player index 2 -> x*16, y*8
player index 3 -> x*16, y*8
```

The runtime trace independently validates both the `8/4` normal scale and the player-1 `20/10` action-state-1 scale against observed `MEPlayer::movePlayer()` position deltas.

The same trace and bounded disassembly now establish the action-state-1 selection path. `Controls::comboJoyStickUpdate()` retains a horizontal direction combo at `Controls +0x110`, while `Controls::tick()` advances the associated `+0x158` timer. A first horizontal activation produces `0x12` on the left or `0x13` on the right. Releasing and then activating the same horizontal sector while the retained timer is strictly greater than `0.03` and strictly less than `0.40` seconds produces fast states `0x0f` on the left or `0x11` on the right. `MEPlayer::setDirComboState()` maps either fast value to `MEPlayer::setActionState(1, false)`.

`MEPlayer::movePlayer(CCPoint)` copies its incoming point into `MEPlayer +0x248`, so this call consumes the resulting live/target position rather than a raw additive joystick delta. The recovered `GameScene` path gets the normalized Controls direction, calls `playerMoveValue()`, combines the result with the current player position, applies the surrounding scene/collision path, and finally calls `movePlayer()`.

`player_motion_contract` models the exact proven per-tick values. `player_direction_combo_contract` retains only the proven horizontal normal/fast sectors and strict timing boundaries; it does not assign action semantics to native cross-direction combo states `0x0e`/`0x10`. `game_scene_player_motion_state` advances action-state `0` or `1` on the project's fixed logical clock, consumes neutral ticks without movement, resets on a new spawn generation, and fails closed for player indices `4..5` until those native branches are separately recovered. `game_scene_player_render_queue` composes the resulting live offset with the recovered first-scene spawn position.

## Current #148 boundary

The project now has evidence-backed pieces for:

- selected Career -> offline player index;
- initial `MEPlayerManager` bootstrap point;
- first-scene event-port marker -> exact offline entry-position transform;
- first EnemyActions frame lookup -> project-owned sprite-frame request;
- initial MeActions body transform -> project-owned player sprite geometry;
- player-frame resolution restricted to the selected Career's shipped default plists;
- player sprite insertion into the first live GameScene render queue at the recovered spawn point;
- recovered fixed joystick geometry and Android touch transport;
- joystick normalized direction and horizontal Controls states `0x12` / `0x13`;
- exact `GameScene::tick()` delivery order into the primary `MEPlayer`;
- exact primary-state -> `CCSprite::setFlipX` mapping;
- exact normal and fast `playerMoveValue()` scales for proven player indices `1..3`;
- recovered same-direction double-tap timing that selects action state `1`;
- fixed-tick live movement composition into the reconstructed player position.

Still required before P8/P9 can be called complete:

- real-device evidence that the reconstructed player is visible and that joystick direction changes both facing and position without crashing;
- recover player-index `4..5` movement branches before enabling movement for those Careers;
- recover scene-border/collision response and camera behavior before treating free movement as a complete GameScene locomotion implementation;
- recover cross-direction combo action semantics, action/frame switching and combat transitions separately rather than assigning names from state numbers alone.
