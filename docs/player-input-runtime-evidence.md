# Player input runtime evidence

This document defines the retained Android proof for the recovered first-GameScene joystick path used by #148. It extends the existing first-scene render proof without weakening its fail-closed requirements.

## Runtime diagnostics

The native runtime now exposes two resource-name-free blocks through the existing diagnostics surface:

```text
GameScene player joystick
captured: false
pointer id: -1
accepted begins: 4
accepted moves: 9
design point: 204,173
power: 0
angle degrees: 0
Player control runtime
generation: 7
primary state: 0x13
flip x: false
```

The joystick counters come from the reconstructed fixed `CCJoyStick` Android input route. The player-control block exposes only the already recovered horizontal contract:

```text
0x12 -> flipX=true
0x13 -> flipX=false
```

It does not infer movement speed, displacement, animation names or combat behavior.

## Device capture

Reach the reconstructed first GameLevels scene with the user-owned imported APK/OBB data, keep the app in the foreground, then run:

```bash
tools/android_player_input_runtime_evidence.sh
```

Set `ANDROID_SERIAL` when more than one adb device is connected. Set `NEVERGONE_EVIDENCE_DIR` to override the default private output directory `artifacts/android-player-input-runtime`.

The harness:

1. refreshes and retains the baseline runtime diagnostics;
2. requires the existing P7 GameScene renderer/staging proof to be valid;
3. chooses the horizontal direction opposite the currently retained player state, guaranteeing that a successful gesture must advance the reconstructed control generation;
4. reads the actual `GameSurfaceView` bounds from uiautomator and maps the recovered 1136x640 fixed joystick center `(204,173)` into physical screen coordinates;
5. injects a normal Android 600 ms swipe from the recovered center to `x +/- 100` design units, which remains inside both the native 165-unit begin radius and 290-unit update radius;
6. retains before/after PNG screenshots and refreshed diagnostics;
7. validates the transition with `tools/android_player_input_runtime_evidence.py`.

The helper intentionally fails closed if the live UI dump does not expose exactly one usable `GameSurfaceView` bounds node.

## Machine acceptance rule

`android_player_input_runtime_evidence.py validate BEFORE AFTER` accepts a transition only when:

- both snapshots independently pass `android_game_scene_runtime_evidence.validate_diagnostics()`, so the proof cannot be taken from a menu, parser-only state or failed GameScene renderer;
- the joystick is released in both retained snapshots and reports pointer id `-1`;
- `accepted begins` increases;
- `accepted moves` increases;
- player-control `generation` increases;
- the final primary state is non-neutral and differs from the baseline state;
- the final facing matches the recovered native contract exactly: `0x12 -> true`, `0x13 -> false`.

The retained PNGs still require visual review before #148 can claim the visible-player requirement complete. The validator proves that a real Android gesture reached the recovered fixed joystick and changed the exact state consumed by the project-owned player render path; it does not perform image recognition or claim that missing player assets became visible.

## Data boundary

Runtime diagnostics contain project-owned state/counters only. Screenshots can contain user-owned original game artwork and therefore remain local with the rest of the device evidence unless a derived, non-proprietary observation is intentionally committed.

## Remaining boundary

A passing real-device capture with the imported player action/atlas data and a visibly flipped reconstructed player is the remaining P8/P9 completion proof. Movement, attack input, enemies and combat remain separate evidence boundaries.
