# Original runtime player-motion evidence: 2026-10-12

## Scope and provenance

This note records derived observations from a user-owned NeverGone 1.0.9 ARMv7 runtime capture and the matching immutable `libcocos2dcpp.so` (SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`). The original APK, video, native trace, screenshots, input log, and game assets are not committed to this repository.

The capture used Frida against the original 32-bit ARM process on a physical Android device. The accepted 80-second run installed 28 symbol-bound hooks and reported zero hook-install errors. It retained 2,478 render-frame boundaries, 23,262 call entries, and matching 23,262 call leaves.

This evidence narrows only the first offline joystick locomotion boundary. It does not establish collision resolution, camera tracking, attack/root motion, vertical physics, or a complete action-state machine.

## Runtime Controls direction

The captured `Controls::onCCJoyStickUpdate(...)` calls establish that the retained `Controls +0x168/+0x16c` pair is the normalized joystick direction vector. Across the captured update samples its magnitude is approximately one whenever the joystick is active.

The same runtime samples independently confirm the already recovered horizontal classification:

- negative normalized X selects primary Controls state `0x12`;
- positive normalized X selects primary Controls state `0x13`;
- `Controls +0x160` remains the separate activity/power-like value used by the native `0.05f` threshold.

The direction vector is therefore not scaled by joystick power before the movement-value function consumes it.

## `MEPlayer::playerMoveValue()` exact scales

A bounded Thumb disassembly of `MEPlayer::playerMoveValue(CCPoint)` at ELF `0x0031d9c0` establishes the per-tick movement values.

For action state `0` and player indices `1..3`:

```text
movement.x = direction.x * 8.0
movement.y = direction.y * 4.0
```

For action state `1`:

```text
player index 1: movement = (direction.x * 20.0, direction.y * 10.0)
player index 2: movement = (direction.x * 16.0, direction.y * 8.0)
player index 3: movement = (direction.x * 16.0, direction.y * 8.0)
```

Two captured runtime pairs independently validate the literal action-state-0/action-state-1 scales:

```text
action 0
  normalized direction ~= (0.945278, 0.326266)
  observed position delta ~= (7.562225, 1.305061)
  expected 8/4 result   ~= (7.562224, 1.305064)

action 1, player index 1
  normalized direction ~= (0.973303, 0.229524)
  observed position delta ~= (19.466064, 2.295235)
  expected 20/10 result ~= (19.466060, 2.295240)
```

The small displayed differences are ordinary float formatting/rounding.

## Fast movement is a timed same-direction reactivation

The same runtime capture resolves how action state `1` is selected; it is not a simple consequence of holding the joystick.

`Controls::comboJoyStickUpdate(float,float)` at ELF `0x0029713c` retains its direction-combo state at `Controls +0x110`. `Controls::tick(float)` at ELF `0x002967a8` increments the `+0x158` timer while the retained direction-combo state is in the `0x0e..0x13` range. The shipped comparisons use these strict timing constants:

```text
fast reactivation: timer > 0.03 and timer < 0.40 seconds
deactivation expiry: timer > 0.50 seconds
```

For the two horizontal sectors used by locomotion:

```text
right sector: -45 < angle < 45
  initial state: 0x13
  same-direction reactivation in the fast window: 0x11

left sector: angle < -135 or angle > 135
  initial state: 0x12
  same-direction reactivation in the fast window: 0x0f
```

`Controls::onCCJoyStickDeactivated()` at ELF `0x002970d4` marks the retained combo as released while preserving the timer/state for a short second-tap window; it clears the retained timer/state only when the timer is already greater than `0.50` seconds.

`MEPlayer::setDirComboState()` at ELF `0x0031df78` accepts both fast horizontal values `0x11` and `0x0f`, updates the player facing direction as needed, and calls `MEPlayer::setActionState(1, false)`. Once selected, action state `1` remains the movement state while the active joystick gesture continues even after the short-lived Controls combo code later clears `+0x110`.

The captured rightward sequence provides a direct dynamic example:

```text
frame 187: first joystick begin
frame 188: right sample creates Controls dir-combo 0x13, timer 0
frame 190: first joystick release, retained timer ~= 0.0386 s
frame 193: second joystick begin, retained 0x13 timer ~= 0.1461 s
frame 194: right sample sees released 0x13 with timer ~= 0.2122 s
frame 195: GameScene receives dir-combo 0x11 and MEPlayer selects action state 1
```

The subsequent captured `MEPlayer::movePlayer()` deltas match the player-index-1 action-state-1 `20x/10y` scale. This independently ties the timed same-direction reactivation to the fast movement branch.

The native cross-direction combo states `0x0e` and `0x10` are visible in the same `comboJoyStickUpdate()` function, but their downstream action semantics are not assigned here. The reconstruction therefore does not reinterpret those branches as fast movement.

## Position and action boundary

`MEPlayer::movePlayer(CCPoint)` copies its incoming point into `MEPlayer +0x248`; the argument is therefore the player's target/live position cursor, not an additive delta API.

The recovered `GameScene` movement path obtains the normalized Controls direction, calls `MEPlayer::playerMoveValue(...)`, combines the returned movement value with the current player position, applies scene/collision constraints, and eventually passes the resulting point to `MEPlayer::movePlayer(...)`.

The existing `MEPlayer::setControlsState(...)` disassembly also proves the minimum normal locomotion transition used here: from idle action state `0x1b`, horizontal primary state `0x12` or `0x13` can select action state `0`. Returning to neutral can return the normal path to `0x1b`, subject to the surrounding native state gates.

## Reconstructed boundary

The project-owned implementation therefore models the bounded subset required for first-scene visible locomotion:

1. Android touch -> recovered fixed joystick geometry;
2. joystick sample -> normalized direction and `0x12`/`0x13` facing state;
3. the already retained fixed 35 Hz project game clock supplies logical movement ticks;
4. ordinary action-state-0 locomotion uses `8x/4y` movement for proven player indices `1..3`;
5. a same-direction second joystick activation inside the recovered strict `0.03..0.40` second window selects action state `1`;
6. action-state-1 movement uses `20x/10y` for player index `1` and `16x/8y` for indices `2..3`;
7. the accumulated live offset is composed with the recovered first-scene spawn point before the player body is drawn.

Neutral logical ticks are consumed without movement so a later joystick activation cannot accumulate a time backlog. A new spawn generation clears the accumulated movement cursor. A release resets the active movement action to normal while preserving only the short native combo window needed to recognize the next same-direction tap.

The implementation intentionally fails closed for player indices `4..5` until their `playerMoveValue()` branches are independently recovered. It also does not yet reproduce cross-direction combo action semantics, scene-border clamping, collision response, camera movement, attack displacement, or vertical physics. The normal/fast horizontal action-state selection described above is now wired to the imported looping walk/run action documents.
