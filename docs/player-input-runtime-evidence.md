# Player input runtime evidence

This document defines the retained Android proof for the recovered first-GameScene joystick path used by #148. It extends the existing first-scene render proof without weakening its fail-closed requirements.

## Runtime diagnostics

The native runtime now exposes resource-name-free joystick, movement and facing blocks through the existing diagnostics surface:

```text
GameScene player joystick
captured: false
pointer id: -1
accepted begins: 4
accepted moves: 9
design point: 204,173
power: 0
angle degrees: 0
GameScene player motion
ready: true
generation: 20
spawn generation: 3
player index: 1
last game tick: 100
applied steps: 10
offset: 0,0
Player control runtime
generation: 7
primary state: 0x13
flip x: false
```

The joystick counters come from the reconstructed fixed `CCJoyStick` Android input route. The player-control block exposes the recovered horizontal contract:

```text
0x12 -> flipX=true
0x13 -> flipX=false
```

The motion block exposes the separately recovered normal locomotion cursor. For the currently proven player-index domain `1..3`, action-state-0 movement consumes the normalized joystick direction at `x*8 / y*4` per logical game tick. The validator does not infer animation names, collision response or combat behavior.

## Device capture

Reach the reconstructed first GameLevels scene with the user-owned imported APK/OBB data, keep the app in the foreground, then run:

```bash
tools/android_player_input_runtime_evidence.sh
```

Set `ANDROID_SERIAL` when more than one adb device is connected. Set `NEVERGONE_EVIDENCE_DIR` to override the default private output directory `evidence/android-player-input-runtime`.

The harness:

1. refreshes and retains the baseline runtime diagnostics;
2. requires the existing P7 GameScene renderer/staging proof to be valid;
3. requires the live player-motion cursor to be ready for the current spawn/player index;
4. chooses the horizontal direction opposite the currently retained player state, guaranteeing that a successful gesture must advance the reconstructed control generation;
5. reads the actual gameplay surface bounds from uiautomator and maps the recovered 1136x640 fixed joystick center `(204,173)` into physical screen coordinates;
6. injects a normal Android 600 ms swipe from the recovered center to `x +/- 100` design units, which remains inside both the native 165-unit begin radius and 290-unit update radius;
7. retains before/after PNG screenshots and refreshed diagnostics;
8. validates both facing and recovered position movement with `tools/android_player_input_runtime_evidence.py`.

The preferred UIAutomator identity is the concrete `GameSurfaceView`/`GLSurfaceView` class. On the tested Android 13 device UIAutomator flattened that custom view to `android.view.View`. When no explicit surface class is exposed, the harness accepts exactly one fallback node only when it is owned by `org.nevergone.recomp`, has no resource id, is focusable, and has positive bounds. Multiple fallback candidates still fail closed; system/navigation views are excluded by the package/id/focusability contract.

## Machine acceptance rule

`android_player_input_runtime_evidence.py validate BEFORE AFTER` accepts a transition only when:

- both snapshots independently pass `android_game_scene_runtime_evidence.validate_diagnostics()`, so the proof cannot be taken from a menu, parser-only state or failed GameScene renderer;
- the joystick is released in both retained snapshots and reports pointer id `-1`;
- `accepted begins` increases;
- `accepted moves` increases;
- player-control `generation` increases;
- the final primary state is non-neutral and differs from the baseline state;
- the final facing matches the recovered native contract exactly: `0x12 -> true`, `0x13 -> false`;
- player motion is ready in both snapshots and the spawn generation/player index remain stable;
- the movement game tick and applied-step count advance;
- horizontal motion offset changes;
- the offset direction agrees with the recovered control hemisphere: state `0x13` must move X positive and state `0x12` must move X negative.

The retained PNGs still require visual review before #148 can claim the visible-player requirement complete. The validator now proves that a real Android gesture reached the recovered fixed joystick, changed the exact facing state, and advanced the evidence-backed reconstructed position cursor; it still does not perform image recognition.

## Data boundary

Runtime diagnostics contain project-owned state/counters only. Screenshots can contain user-owned original game artwork and therefore remain local with the rest of the device evidence unless a derived, non-proprietary observation is intentionally committed.

## Remaining boundary

A passing real-device capture with imported player action/atlas data plus before/after PNGs that visibly show the reconstructed player at different horizontal positions/facing is the remaining P8/P9 proof. Scene collision/border response, camera following, attack/root motion, animation switching, enemies and combat remain separate evidence boundaries.
