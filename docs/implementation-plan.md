# Implementation plan

This plan turns the reverse-engineering work into incremental, testable milestones. The project should remain buildable at each milestone and should never require committing proprietary game assets or original binaries.

## Phase A — Research baseline

### A1. APK inventory
- inventory DEX, native libraries and assets;
- record hashes and ABI information;
- document the original manifest/application entry point;
- identify engine and third-party libraries.

### A2. Java/JNI map
- enumerate Java classes that participate in startup;
- enumerate exported JNI methods;
- separate stock Cocos2d-x bridge methods from game-specific methods;
- reconstruct the minimal Java/JNI surface in the new Android shell.

### A3. Native symbol map
- export and demangle dynamic symbols;
- index RTTI/vtables;
- identify `AppDelegate` and startup callees;
- classify stock Cocos2d-x versus game-specific symbols.

**Exit criterion:** enough information to trace startup without guessing.

## Phase B — Modern build skeleton

### B1. Clean Android shell
- current SDK/AGP project;
- Java entry activity;
- CMake/NDK native library;
- `arm64-v8a` and `armeabi-v7a` targets;
- runtime ABI/page-size probe.

### B2. Runtime foundation
- logging abstraction;
- asset-provider abstraction;
- lifecycle bridge;
- input and surface interfaces;
- deterministic startup diagnostics.

**Exit criterion:** a project-owned APK loads project-owned native code on modern Android.

## Phase C — Script/resource recovery

### C1. Lua loader
- locate `.lua` file reads;
- trace the transform before `luaL_loadbufferx`/`luaL_loadfilex`;
- reconstruct decoder in a standalone tool;
- validate decoded output structurally without committing it.

### C2. HPC configuration
- locate `.hpc` loader;
- determine compression/encryption/serialization;
- create a parser/validator;
- document schemas incrementally.

### C3. Asset preparation
- local extractor for a user-supplied original APK;
- manifest/hash verification;
- stable local working layout ignored by Git.

**Exit criterion:** the new runtime can consume locally prepared original resources.

## Phase D — Cocos2d-x compatibility layer

### D1. Upstream matching
- use Cocos2d-x 2.1.2 as the reference baseline;
- identify exact modified engine areas;
- avoid reconstructing stock upstream code manually.

### D2. Modernization
- make required engine subset compile with current Clang/NDK;
- replace removed Android/NDK APIs;
- ensure 64-bit clean pointer/integer handling;
- ensure 16 KiB page-size-safe loading/alignment.

### D3. Platform replacements
- replace obsolete FFmpeg integration where practical;
- replace deprecated billing/network/platform services with optional adapters;
- keep offline startup independent of removed services.

**Exit criterion:** engine startup can run without original `libcocos2dcpp.so`.

## Phase E — Game startup reconstruction

### E1. AppDelegate
- reconstruct constructor/destructor where required;
- reconstruct `applicationDidFinishLaunching` behavior;
- restore search paths and script bootstrap;
- reach the first game/loading scene.

### E2. Data managers
Prioritize managers used during boot:
- `DataManager`;
- `LogicManager`;
- configuration managers;
- save data;
- shader/audio initialization.

### E3. Login/offline flow
- recreate enough UI/script bindings to reach the first interactive menu;
- stub unavailable online services behind explicit adapters rather than hard failures.

**Exit criterion:** boot to menu using only recompiled/reconstructed code.

## Phase F — Gameplay

- player and enemy objects;
- state machines;
- combat managers;
- animation/action data;
- audio;
- save/load compatibility;
- scene transitions;
- controller/touch input.

**Exit criterion:** a representative gameplay scene works end-to-end.

## Phase G — Validation and release engineering

- behavior comparison against the original APK;
- deterministic regression tests for parsers/decoders;
- Android 64-bit-only testing;
- 16 KiB page-size testing;
- package/signing/release documentation;
- reproducible build instructions.

## Immediate work order

1. Merge the M0/M1 baseline and Android shell.
2. Trace the Lua decode/load path.
3. Add a standalone Lua decode validator.
4. Add local original-APK extraction/verification tooling.
5. Start Cocos2d-x 2.1.2 symbol matching around `AppDelegate`.
6. Replace the shell status screen with the first reconstructed engine lifecycle path.
