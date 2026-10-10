# Android runtime validation matrix

This document separates CI transport evidence from release-device evidence. Passing an x86_64 emulator job does not establish arm64 behavior or a 16 KiB page-size runtime.

## Automated emulator transport

`.github/workflows/android-emulator-smoke.yml` exercises the project-owned Android shell on stock x86_64 emulator images for API 35 and API 36.

For each API level the workflow now performs, in order:

1. install the emulator-only x86_64 debug APK;
2. launch `MainActivity` and require a live foreground process;
3. press Home and require the same process to survive the pause;
4. resume the activity and require the same PID plus foreground state;
5. finish the activity with Back, immediately relaunch it and require the same process, forcing a new Activity/`GLSurfaceView`/EGL-surface lifecycle inside the retained native process;
6. force-stop the package and verify the process is actually gone;
7. cold-launch the package again and require a healthy foreground process;
8. reject any phase that records a Java `FATAL EXCEPTION` or native fatal signal for `org.nevergone.recomp`.

The same workflow remains scheduled twice weekly and can be dispatched manually. A pull request that changes the workflow file also runs the API 35/36 matrix so lifecycle changes validate themselves before merge.

The native renderer already reports pause count, resume count and surface generation. GameScene diagnostics additionally report asset-staging and static-renderer state, so device/manual runs can distinguish lifecycle failure from scene-data failure.

## Current release evidence

| Target / property | Evidence | Release status |
| --- | --- | --- |
| Android API 35, x86_64 | launch + pause/resume + same-process Activity/GL-surface recreation + cold restart emulator workflow | CI transport only |
| Android API 36, x86_64 | launch + pause/resume + same-process Activity/GL-surface recreation + cold restart emulator workflow | CI transport only |
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
