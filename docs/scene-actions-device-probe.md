# Debug SceneActions device probe

The debug APK contains an opt-in device probe for the reconstructed current-scene `klhuo` SceneActions pair. Release builds do not register the probe application class.

This is a validation hook, **not** recovered game logic. Supplying a variant manually means “treat the native spatial trigger as already confirmed and treat this exact variant as already selected.” The unresolved `showEventObjTip` / `isContainsRect` trigger condition and global PRNG ordering remain outside this probe.

## Start

Install a debug APK with the original APK/OBB assets imported, then launch the normal activity with an explicit recovered variant:

```bash
adb shell am force-stop org.nevergone.recomp
adb shell am start \
  -n org.nevergone.recomp/.MainActivity \
  --ei nevergone_scene_actions_variant 1
```

Continue through the normal game flow. The probe retries once per second until a unique current-scene `klhuo-1.png` pair is bound and its selected variant can be staged. It then:

1. calls the existing explicit `SceneActionLiveRuntime.start()` on the GLSurfaceView thread;
2. submits the born and idle frame-zero visuals;
3. advances both slots using Choreographer elapsed time;
4. stages each exact requested frame before GLES submission;
5. stops advancing born after its recovered completion edge;
6. keeps idle running through the recovered frame-zero loop.

The per-frame delta is capped at 250 ms in this debug driver to prevent a debugger/app stall from injecting an unbounded catch-up step. This cap is probe policy only; it is not claimed as original-game timing.

## Observe

```bash
adb logcat -s NeverGoneSceneActionProbe
```

Expected milestones include:

```text
explicit SceneActions probe enabled for variant 1
SceneActions probe started for current-scene variant 1
born SceneActions slot completed
idle SceneActions slot looped to frame zero
```

A stage/submit/tick failure is logged and the explicit request remains armed for a later retry. Variant `4` is intentionally allowed because the recovered float32 PRNG conversion can produce it; staging still fails closed when the imported game data does not contain the corresponding pair.

## Disable

Restart the normal activity without the debug extra:

```bash
adb shell am force-stop org.nevergone.recomp
adb shell am start -n org.nevergone.recomp/.MainActivity
```

Pausing or destroying `MainActivity` also resets the probe runtime. `MainActivity.onPause()` has already stopped the GLSurfaceView before the debug lifecycle callback runs, so the reset is synchronous and cannot be deferred into a later resumed GL frame.
