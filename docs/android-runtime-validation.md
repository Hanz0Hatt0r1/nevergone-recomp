# Android runtime validation matrix

This document separates CI transport evidence from release-architecture evidence. A standard x86_64 emulator does not establish arm64 behavior or 16 KiB page-size compatibility; the 16 KiB property has a dedicated Android 15 `google_apis_ps16k` gate.

## Automated emulator transport

`.github/workflows/android-emulator-smoke.yml` exercises the project-owned Android shell on stock x86_64 emulator images for API 35 and API 36. The workflow uses GitHub-hosted Linux KVM acceleration whenever `/dev/kvm` is exposed by the selected runner and logs an explicit software-emulation fallback otherwise.

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

## Dedicated 16 KiB runtime

`.github/workflows/android-16k-emulator-smoke.yml` uses the Android 15 `google_apis_ps16k` x86_64 system image and enables KVM when the hosted runner exposes it. Before installing the app, `tools/android_16k_runtime_smoke.sh` requires:

```text
adb shell getconf PAGE_SIZE == 16384
adb shell getprop ro.product.cpu.abi == x86_64
```

Only after both runtime properties are confirmed does it execute the same launch, native lifecycle-counter, GL-surface-recreation and cold-restart smoke used by the standard API matrix. A successful full job is actual 16 KiB runtime evidence rather than an inference from ELF or ZIP alignment.

A previous software-emulated run confirmed `PAGE_SIZE=16384` and the expected x86_64 ABI but did not complete the lifecycle contract because Android's ActivityManager became unavailable. That partial result is not treated as a passing compatibility gate.

Even after a successful 16 KiB x86_64 run, the result proves page-size compatibility of this native/app runtime in CI, not arm64 runtime behavior; release-architecture validation remains a separate gate.

GameScene diagnostics additionally report asset-staging and static-renderer state, so device/manual runs can distinguish lifecycle failure from scene-data failure.

## Current release evidence

| Target / property | Evidence | Release status |
| --- | --- | --- |
| Android API 35, x86_64 | lifecycle workflow with native pause/resume counters, same-process renderer surface-generation increase and cold restart; KVM used when available | gate pending current successful run |
| Android API 36, x86_64 | lifecycle workflow with native pause/resume counters, same-process renderer surface-generation increase and cold restart; KVM used when available | gate pending current successful run |
| Android 15, x86_64, 16 KiB pages | `google_apis_ps16k`; page size and ABI confirmed in a prior run; full lifecycle gate required | partial runtime evidence; lifecycle pass pending; not arm64 |
| `armeabi-v7a` APK | build target and ELF/package checks | build evidence only |
| `arm64-v8a` APK | build target and ELF/package checks | build evidence only; runtime pending |
| 16 KiB ELF/ZIP alignment | Android build validation | build evidence only until the dedicated runtime gate passes |
| arm64 physical/emulated runtime | no accepted release-gate run yet | pending |

## What closes issue #149

A successful standard x86_64 API 35/36 lifecycle matrix establishes Android lifecycle transport. A successful dedicated x86_64 `google_apis_ps16k` job establishes actual 16 KiB page-size runtime compatibility. Issue #149 remains open until release-architecture evidence is also recorded:

- an arm64 target installs and launches the normal arm64 APK;
- pause/resume and surface recreation are exercised on the release architecture;
- the tested API/device/ABI/page-size combination and diagnostics are retained as release evidence.

Build-time alignment remains necessary, but is not treated as runtime proof.
