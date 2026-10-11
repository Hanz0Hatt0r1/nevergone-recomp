# Android runtime validation matrix

This document separates CI transport evidence from release-device evidence. Passing an x86_64 emulator job does not establish arm64 behavior or a 16 KiB page-size runtime.

## Automated emulator transport

`.github/workflows/android-emulator-smoke.yml` exercises the project-owned Android shell on stock x86_64 emulator images for API 35 and API 36.

For each API level the workflow performs, in order:

1. install the emulator-only x86_64 debug APK;
2. launch `MainActivity` and require a live foreground process;
3. refresh the existing runtime diagnostics UI and require a nonzero native `resume events` count and `GL surface generation`;
4. press Home and require the same process to survive the pause;
5. resume the activity, require the same PID plus foreground state, then require native `resume events` and `pause events` to have increased;
6. finish the activity with Back, immediately relaunch it and require the same process, forcing a new Activity/`GLSurfaceView` lifecycle inside the retained native process;
7. refresh diagnostics again and require another resume/pause transition plus an increased `render_bridge` `GL surface generation`;
8. force-stop the package and verify the process is actually gone;
9. cold-launch the package again and require a healthy foreground process plus a valid new-process renderer baseline;
10. reject any phase that records a Java `FATAL EXCEPTION` or native fatal signal for `org.nevergone.recomp`.

The counter proof does not add a debug-only native entry point. The workflow taps the existing `Refresh runtime diagnostics` button, reads the normal `TextView` through `uiautomator`, and parses the same `nativeBootstrapInfo()` report available to a manual tester. `tools/android_runtime_diagnostics.py` fails closed on missing, partial or ambiguous counter data.

The relevant counters come from `render_bridge`, not `app_delegate_state`: `resume events`, `pause events`, and the renderer's `GL surface generation`. The latter increments on every renderer `on_surface_created()` callback and is therefore the counter used to prove same-process GL-surface recreation.

The same workflow remains scheduled twice weekly and can be dispatched manually. A pull request that changes the workflow, lifecycle script or diagnostics parser also runs the API 35/36 matrix so lifecycle changes validate themselves before merge.

GameScene diagnostics additionally report asset-staging and static-renderer state, so device/manual runs can distinguish lifecycle failure from scene-data failure.

## Current release evidence

| Target / property | Evidence | Release status |
| --- | --- | --- |
| Android API 35, x86_64 | launch + native pause/resume counters + same-process renderer surface-generation increase + cold restart emulator workflow | CI transport only |
| Android API 36, x86_64 | launch + native pause/resume counters + same-process renderer surface-generation increase + cold restart emulator workflow | CI transport only |
| `armeabi-v7a` APK | build target and ELF/package checks | build evidence only |
| `arm64-v8a` APK | build target and ELF/package checks | build evidence only; runtime pending |
| 16 KiB compatibility | ELF/ZIP alignment checks in Android build validation | build evidence only; 16 KiB runtime pending |
| arm64 physical/emulated runtime | no accepted release-gate run yet | pending |
| 16 KiB page-size runtime | no accepted release-gate run yet | pending |

## What closes issue #149

The x86_64 API 35/36 lifecycle matrix closes only the launch/lifecycle transport portion of #149. The issue remains open until all of the following are recorded:

- an arm64 target installs and launches the normal arm64 APK;
- pause/resume and surface recreation are exercised on the release architecture;
- a target actually running with 16 KiB pages launches the application successfully;
- the tested API/device/ABI/page-size combination and diagnostics are retained as release evidence.

Build-time alignment is necessary for the 16 KiB target but is not runtime proof.
