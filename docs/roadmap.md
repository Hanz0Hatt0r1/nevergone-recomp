# Never Gone recompilation roadmap

This document is the working implementation plan for `nevergone-recomp`. It is intentionally ordered around evidence and testable milestones rather than attempting a full rewrite at once.

## Development principles

1. Preserve evidence before reconstruction: hashes, symbols, strings, JNI signatures, resource names and runtime observations should be recorded before code is rewritten.
2. Separate known third-party code from Never Gone-specific code as early as possible.
3. Keep every reconstructed subsystem buildable and testable in isolation.
4. Prefer small reproducible tools over one-off manual analysis.
5. Do not commit proprietary game binaries or assets. Analysis metadata, hashes, scripts and original reimplementation code are allowed.

## Current baseline

Confirmed from the original Android APK:

- package: `com.hippiegame.nevergone`;
- one `classes.dex`;
- native ABI: `armeabi-v7a` only;
- native libraries: `libcocos2dcpp.so` and `libffmpeg.so`;
- the main game library is a stripped ARM EABI5 ELF shared object;
- engine version string: `cocos2d-2.1rc0-x-2.1.2`;
- Lua runtime string: Lua 5.2.3;
- JNI includes stock Cocos2d-x bridges plus Never Gone billing callbacks;
- DEX also declares a `com.ngds.cocos.GamepadBridge` native interface;
- game data is heavily asset-driven and contains Lua, CSV, HPC, plist, audio and video files.

## M0 — Reproducible research baseline

Status: **in progress**

Deliverables:

- [x] repository and project README;
- [x] reproducible APK inventory scanner;
- [x] original APK SHA-256 recorded;
- [x] native library hashes recorded;
- [x] exact Cocos2d-x version string identified;
- [x] initial asset inventory;
- [x] initial JNI inventory;
- [ ] decoded Android manifest committed as analysis notes;
- [ ] Java/DEX class map;
- [ ] reproducible symbol/RTTI extraction report;
- [ ] initial Ghidra project/import instructions.

Exit condition: another developer can take the same APK and reproduce the same inventory without manual guesswork.

## M1 — Native code map

Status: **started**

Tasks:

- identify `JNI_OnLoad` behavior and any `RegisterNatives` tables;
- map `AppDelegate::applicationDidFinishLaunching()` and startup call graph;
- enumerate RTTI-backed game classes;
- classify symbols into engine, third-party and Never Gone-owned namespaces/classes;
- locate resource-loading, save-data, networking, payment and gamepad entry points;
- identify the boundary between Cocos2d-x 2.1.2 source and game-specific modifications;
- import the library into Ghidra with consistent names and data types;
- export function names/addresses into machine-readable metadata.

Priority classes currently visible in RTTI/symbols:

- `AppDelegate`
- `DataManager`
- `LogicManager`
- `GameSaveData`
- `LoadingLayer`
- `LoginScreen`
- `GameSceneUI`
- `EnemyObject`
- `EquipManager`
- `GuideManager`
- `NPCManager`
- `PaymentMgr`
- `SockClient` / `SockServer`
- `NGGamepadListener`

Exit condition: startup and the major gameplay subsystems have named functions/classes and known relationships in the analysis database.

## M2 — Resource pipeline

Status: **not started**

Tasks:

- classify all 107 `.lua` files as source, bytecode or encoded payloads;
- determine the `.hpc` format and whether it is serialized, compressed or encrypted;
- document CSV schemas and ownership of each table;
- identify texture atlas/plist conventions;
- trace resource search paths from `AppDelegate::AddAllSearchPath()`;
- identify runtime patch/update behavior and `version.ini` handling;
- write extract/inspect/conversion tools for custom formats;
- map asset names to native classes that consume them.

Exit condition: all resource classes needed to reach the main menu can be read by project-owned tooling.

## M3 — Modern build skeleton

Status: **not started**

Create a clean Android project that does not contain original proprietary binaries.

Targets:

- Gradle project;
- CMake-based NDK build;
- modern Android SDK target;
- `armeabi-v7a` debug target for behavioral comparison;
- `arm64-v8a` as the primary modern target;
- reconstructed Java/Kotlin bootstrap;
- C++ `AppDelegate` skeleton;
- logging and crash diagnostics from the beginning.

Initial source layout:

```text
android/
src/
  game/
  platform/android/
  reconstructed/
third_party/
cmake/
tests/
```

Exit condition: the project installs, loads its own native library and enters a reconstructed C++ startup function on both ARMv7 and ARM64 builds.

## M4 — Boot to menu

Status: **not started**

Reconstruct only the path required to reach the first interactive screen:

1. Android activity creation;
2. GL surface and renderer initialization;
3. Cocos2d-x director setup;
4. search paths and resource manager;
5. configuration/localization loading;
6. loading scene;
7. login/start scene;
8. menu input and audio.

External services should initially be stubbed behind interfaces so Google Play billing, analytics or dead backend dependencies cannot block boot.

Exit condition: a clean rebuild reaches a functional local menu using user-supplied original assets.

## M5 — Gameplay reconstruction

Status: **not started**

Subsystem order:

1. scene/world initialization;
2. player and enemy object model;
3. input/gamepad abstraction;
4. combat and damage;
5. animation and effects;
6. inventory/equipment;
7. progression and level data;
8. save/load;
9. audio/video playback;
10. optional networking/service compatibility.

Each subsystem should have comparison notes against the original ARMv7 build.

Exit condition: representative offline gameplay is playable from a clean recompilation.

## M6 — Compatibility and preservation release

Status: **not started**

Tasks:

- ARM64 validation;
- 16 KiB page-size compatibility;
- current Android storage model;
- current audio/input/lifecycle behavior;
- replace obsolete FFmpeg integration with a maintainable equivalent where required;
- remove hard dependencies on unavailable online services;
- deterministic/reproducible build instructions;
- save compatibility tests;
- regression suite and device matrix.

Exit condition: maintained release builds run on current Android devices without the original 32-bit native executable.

## Immediate work queue

The next implementation sequence is:

1. add a DEX native-method mapper and compare Java declarations with exported JNI symbols;
2. add a native symbol/RTTI report generator;
3. produce `docs/apk-analysis.md` and `docs/jni-map.md` from the original APK;
4. trace `JNI_OnLoad` and the gamepad registration path;
5. map `AppDelegate::applicationDidFinishLaunching()` in Ghidra;
6. identify the first game scene created from that path;
7. begin resource format triage, starting with Lua and HPC files;
8. create the modern Android/CMake skeleton once the startup boundary is sufficiently understood.
