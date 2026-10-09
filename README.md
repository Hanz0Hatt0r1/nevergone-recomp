# Never Gone Recomp

Experimental clean-room reverse-engineering and recompilation project for the Android version of **Never Gone**.

The goal is to reconstruct the original game runtime as maintainable source code for modern Android, including `arm64-v8a`, without depending on the obsolete original Android toolchain and without redistributing proprietary game binaries or assets.

> [!IMPORTANT]
> The project is **not yet a playable recompilation**. It is well beyond the research-only stage: the repository contains a modern Android/NDK shell, project-owned native runtime code, embedded Lua 5.2.3, user-owned APK/OBB import and decoding workflows, reconstructed startup/login state, bounded binary-scene parsers, and an increasingly complete clean-room reconstruction of the original `ChooseHero` character-selection presentation.

## Current state

As of **2026-10-09**, the current `main` branch includes:

- a modern Gradle/CMake Android application;
- project-owned C++/JNI runtime code;
- `armeabi-v7a` and `arm64-v8a` build targets;
- embedded **Lua 5.2.3**, matching the runtime identified in the original binary;
- deterministic decoding for transformed Lua/PNG/HPC/CSV resources;
- Android import flows for user-owned original APK and OBB data;
- reconstructed Android → JNI → startup state and lifecycle forwarding;
- reconstructed `AppDelegate` startup-state semantics and splash/scene-generation handling;
- a verified clean-room `HelloWorld::createUI()` → `ManagementLayer::initLoginLayer()` transition;
- callback-driven `ManagementLayer` routes for announcement, server selection, role selection, role creation and enter-game state;
- structured server-list and role-list payload decoding into project-owned C++ models;
- reconstructed `NewServerList` selection semantics, including exact `LastLoginServer` preselection, recovered tap-vs-drag behavior and one-shot enter requests;
- reconstructed Lua/native startup bindings and filesystem/search-path behavior;
- host-side probes for startup, login state, ChooseHero behavior and binary parsers;
- recovered/staged TexturePacker metadata and user-owned assets for the `ChooseHero` route;
- reconstructed `PartThree()` dark-cloud background fade;
- reconstructed `BalckCloud()` foreground-cloud composition and repeated motion;
- reconstructed ChooseHero thunder/lightning scheduling contracts and effect composition;
- interactive ChooseHero role-item rendering and touch routing;
- reconstructed selected-role focus-highlight timing and rendering;
- bounded parsing of standalone `DMG_01.sData` / `DMG_02.sData` hero save metadata needed by the standalone ChooseHero pane;
- login-localization CSV resolution for recovered profile labels;
- Android-side rasterization and native composition of localized ChooseHero profile text (`name`, `level`, `play time`);
- bounds-checked `HPData` reading with recovered `HPRange` semantics;
- verified `GameLevels::LoadGL_Scene()` parsing through the first object-record prefix;
- CI coverage for the modern Android build, Lua compatibility, startup/login state, ChooseHero visual/state contracts, standalone save metadata, localized profile text, scene parsing and page-size validation.

The critical path is now the **visible offline character-selection path**, not bootstrap discovery. The next work is to finish the remaining ChooseHero presentation/interaction gaps, extend verified scene-data parsing only where binary evidence supports it, and reach the next offline scene without loading the original `libcocos2dcpp.so`.

## Goals

- Reverse engineer the original Android APK and native runtime.
- Recover and document the Android → JNI → Cocos2d-x → Lua startup flow.
- Separate upstream engine/library code from Never Gone-specific code.
- Reconstruct the script/resource pipeline from user-supplied original files.
- Recreate game-specific native systems as maintainable clean-room source.
- Restore the offline game flow before attempting obsolete online services.
- Produce reproducible modern Android builds.
- Support `arm64-v8a`, modern Android versions and 16 KiB page-size devices.
- Keep offline behavior as close to the original release as practical.

## Established findings

The reconstruction work has established that:

- package: `com.hippiegame.nevergone`;
- original version: `1.0.9`;
- original native ABI: **`armeabi-v7a` only**;
- original native libraries: `libcocos2dcpp.so` and `libffmpeg.so`;
- the main library identifies its engine as **`cocos2d-2.1rc0-x-2.1.2`**;
- the embedded Lua runtime is **Lua 5.2.3**;
- the APK contains **107 `.lua` modules** transformed with the same reversible asset transform used by several PNG/HPC/CSV resources;
- the exact shipped `cocos2d::Decode` transform and key have been recovered;
- all 107 Lua payloads can be decoded locally from a user-owned APK;
- the static Lua graph contains 107 modules, 104 resolved edges and 101 modules reachable from `Game.StartLua`;
- the original `libcocos2dcpp.so` retains roughly **26,875 defined function symbols**;
- the manifest launcher is `com.hippiegame.nevergone.TJ_P_01` and its critical launcher/native lifecycle path is mapped;
- `AppDelegate::AddAllSearchPath()` and its 59 child search directories have been recovered;
- startup reaches `Game.StartLua`, `Game.ClientRequire`, `ShareLogic.require`, `HelloWorld` and `ManagementLayer` through known paths;
- `HelloWorld::createUI()` synchronously reaches `ManagementLayer::initLoginLayer()` in the recovered startup path;
- recovered callbacks route announcement, server list, role list, role creation and enter-game events into distinct reconstructed Management states;
- original `NewServerList` behavior preselects only an exact `LastLoginServer` id match, rejects row selection when vertical movement exceeds 10 pixels and confirms through the equivalent of `g_UILogin.EnterGameLogicServer(ip, id)`;
- the shipped `ChooseHeroBackground::createUI()` effectively dispatches the `PartThree()` path;
- `PartThree()` creates the dark-cloud background, lightning/thunder effect nodes and ground-light nodes, with the background fading in over six seconds;
- `BalckCloud()` creates six foreground clouds with recovered anchors, opacity, positions and repeat periods;
- the ChooseHero thunder path uses the process-global libc `lrand48()` stream with recovered transforms for delay, fade durations, lightning selection and optional sound selection;
- ChooseHero role items now have reconstructed project-owned rendering, selection/touch behavior and a recovered selected-item focus effect;
- the standalone ChooseHero pane can read the bounded shipped save prefix needed to obtain slot metadata and combine it with recovered localization rows;
- localized profile text is composed in the Android/native pipeline before the selected-item focus layer, preserving the reconstructed scene/UI ordering;
- the original 32-bit `HPRange` used by `HPData::getBytes(...)` is `{byte_offset, byte_length}`;
- `GameLevels::LoadGameLevels()` loads scene, action, global and port-node sections in a recovered fixed order;
- the verified `LoadGL_Scene()` boundary reaches the first layer and the first object's immediately sequential `int32`/`uint32` prefix, then stops before an unresolved-width `char*` read.

## Project status

| Area | Status |
| --- | --- |
| Original APK / manifest / signing baseline | Done |
| Engine and Lua runtime identification | Done |
| Native symbol/JNI inventory | Substantially mapped |
| Asset/Lua transform recovery | Done |
| Lua decoder and dependency mapping | Done |
| Modern Gradle/CMake application | Done |
| Project-owned JNI/native runtime | Done |
| Lua 5.2.3 integration | Done |
| User-owned APK and OBB import | Done |
| Startup compatibility layer | In progress, substantial coverage |
| Filesystem/search-path reconstruction | Early startup path covered |
| `AppDelegate` + lifecycle state | Implemented and host-tested |
| Splash / initial UI transition | Implemented and generation-aware |
| Management callback routing | Implemented |
| Structured server/role payload decoding | Implemented and host-tested |
| `NewServerList` selection semantics | Implemented and host-tested |
| Visible server-selection UI | In progress |
| ChooseHero background / `PartThree()` | Implemented |
| ChooseHero `BalckCloud()` motion | Implemented and host-tested |
| ChooseHero thunder/lightning contracts | Implemented; integration still being extended |
| ChooseHero role-item rendering/input | Implemented |
| ChooseHero selected-item focus highlight | Implemented and tested |
| Standalone hero save metadata reader | Implemented and bounded |
| Localized ChooseHero profile labels | Implemented and CI-tested |
| Role-selection / character interaction | Substantially reconstructed; not end-to-end complete |
| `HPData` / `HPRange` reconstruction | Reader foundation done; range semantics verified |
| `GameLevels` binary format reconstruction | In progress; first object prefix verified |
| Rendering/input reconstruction | In progress; current critical path |
| Original login/title/role flow | Not yet complete end-to-end |
| Offline gameplay | Not yet functional |
| `arm64-v8a` build configuration | Done |
| 16 KiB page-size build validation | Covered in CI; runtime/device validation remains |
| Android 15/16 runtime validation | Pending |
| Playable recompilation | Not yet |

## Reconstruction strategy

The project does **not** attempt a blind line-by-line rewrite of the original `libcocos2dcpp.so`.

The current strategy is dependency-driven and evidence-driven:

1. recover static metadata, symbols, DEX/JNI relationships and script dependencies;
2. decode user-owned resources locally with reproducible tooling;
3. execute recovered startup scripts against a clean modern runtime;
4. reconstruct state machines at verified semantic boundaries rather than copying the original ABI;
5. convert recovered callback/save/resource payloads into bounded project-owned models before attaching them to renderer state;
6. use tracebacks, missing globals, binary call order and behavior differences to identify the next compatibility surface;
7. reconstruct animation/timeline behavior from verified call order, parameters and random transforms rather than visual guesswork;
8. reconstruct binary readers only to proven field boundaries and stop at unresolved widths/semantics;
9. recreate only the native/game behavior required by the reachable offline path;
10. preserve offline behavior while replacing or stubbing obsolete service integrations where necessary.

See [`docs/roadmap.md`](docs/roadmap.md) for the detailed plan.

## Repository layout

```text
nevergone-recomp/
├── README.md
├── android/              # modern Gradle/NDK app and clean-room runtime
│   ├── app/
│   └── README.md
├── docs/                 # reverse-engineering evidence and reconstruction notes
├── tools/                # decoders, probes, Ghidra helpers and validation tools
├── third_party/_local/   # ignored locally fetched dependencies
└── .github/              # CI workflows
```

Original Never Gone APK/OBB files, decoded proprietary scripts, save files and copyrighted assets are intentionally not stored in the repository.

## Modern Android runtime

The clean-room Android shell under [`android/`](android/) can already:

- target a current Android SDK;
- build/load project-owned native C++ code for `armeabi-v7a` and `arm64-v8a`;
- report ABI, pointer width and runtime page size;
- maintain an app-local runtime root and privacy-safe persistent UUID;
- embed and execute Lua 5.2.3;
- register reconstructed startup globals and compatibility bindings;
- load decoded/imported Lua modules from app-private storage;
- import original user-owned APK/OBB resources through Android storage flows;
- forward Android lifecycle events into reconstructed `AppDelegate` state;
- preserve scene-generation state across reloads;
- transition through the verified `HelloWorld` → `ManagementLayer` boundary;
- retain route-scoped structured server/role state;
- maintain reconstructed server selection and one-shot enter requests;
- stage and render recovered ChooseHero background/effect/role-item assets;
- route ChooseHero touches through the project-owned role-selection state;
- render the recovered selected-role focus effect;
- read bounded standalone hero save metadata from user-owned runtime files;
- resolve recovered login-localization CSV rows and rasterize profile labels on Android;
- compose profile labels natively in the reconstructed ChooseHero layer order;
- locate imported `gamescene/gs_list/pvp_scene.glData` and validate it through the currently verified parser boundary.

It does **not** link against the original `libcocos2dcpp.so`.

See [`android/README.md`](android/README.md) for build/runtime details.

## Recovered login / role flow

```text
reconstructed AppDelegate startup state
        |
        | initial-ui-ready
        v
HelloWorld::createUI()
        |
        | verified synchronous transition
        v
ManagementLayer::initLoginLayer()
        |
        | callback-driven route changes
        +--> announcement
        +--> server-selection
        +--> role-selection
        +--> role-created
        `--> entering-game
```

The route state is generation-aware so stale callbacks/assets from a previous reconstructed scene generation cannot silently activate a new one.

### Server selection

The project-owned replacement preserves the recovered `NewServerList` semantics:

- exact `LastLoginServer` id match for preselection;
- touch movement greater than 10 pixels is treated as drag rather than row selection;
- selected record state is independent from rendering;
- confirmation emits one enter request with the selected server id/name/ip.

### ChooseHero role presentation

The reconstructed ChooseHero path now includes more than the background/effect layer. It also contains:

- recovered role-tile assets and placement;
- project-owned role-item composition;
- touch routing and selection state;
- selected-item focus streaks/timeline;
- standalone save metadata loading for the two shipped hero slots;
- recovered localization lookup;
- Android-rasterized profile labels for hero name, level and play time;
- native profile composition before the focus layer.

This is still a clean-room semantic reconstruction, not ABI compatibility with the original Cocos2d-x widget classes.

## Resource recovery

Decode supported transformed assets locally from a legally obtained APK:

```bash
python3 tools/asset_decoder.py /path/to/com.hippiegame.nevergone.apk \
  --output build/decoded-assets
```

Recovered proprietary output should remain local and must not be committed.

The Android app also supports user-owned original resource import into app-private runtime storage, including OBB data, ChooseHero atlases/effects, save-derived metadata inputs and binary scene data required by later reconstructed paths.

## Lua 5.2.3

The repository does not vendor upstream Lua source directly. Fetch and verify the pinned release with:

```bash
python3 tools/fetch_lua_5_2_3.py
```

This creates the ignored local directory:

```text
third_party/_local/lua-5.2.3/
```

## Build

Requirements:

- JDK 17+
- Android SDK Platform 36
- Android NDK
- CMake 3.22.1+
- Gradle compatible with Android Gradle Plugin 9.4
- Python 3

Prepare Lua and build:

```bash
python3 tools/fetch_lua_5_2_3.py
cd android
gradle wrapper
./gradlew assembleDebug
```

The debug APK is normally produced under:

```text
android/app/build/outputs/apk/debug/
```

## Host-side probing and CI

The repository uses clean-room host probes and synthetic data where practical so proprietary resources do not need to live in source control.

Coverage includes:

- reconstructed Lua startup behavior;
- `AppDelegate` startup/lifecycle state;
- initial UI transition and Management routes;
- structured server/role payload parsing;
- server-selection semantics;
- ChooseHero background/cloud/effect timelines;
- role-item mapping and interaction;
- selected-item focus behavior;
- bounded standalone hero save metadata parsing;
- login-localization resolution and localized profile-text composition;
- `HPData` / `GameLevels` parser regressions;
- Android/NDK compilation and page-size checks.

## HPData / GameLevels reconstruction

The project has a clean-room foundation for the binary scene format used by `GameLevels`.

Recovered evidence establishes that the original `HPRange` passed by value into `HPData::getBytes(...)` consists of a byte offset followed by a byte length. The replacement uses explicit bounds-checked offsets, lengths and a transactional sequential cursor.

For `GameLevels::LoadGL_Scene()` the verified parser currently reaches:

1. one unresolved signed 32-bit top-level value;
2. scene count;
3. first-scene bounded string prefix and point values;
4. scene layer count;
5. first layer header and object count;
6. first object's immediately sequential `int32` then `uint32` prefix.

The parser deliberately stops before the next unresolved-width `char*` read. New fields are added only when the original control flow makes their width/order sufficiently clear.

See [`docs/hpdata-gamelevels.md`](docs/hpdata-gamelevels.md).

## Major milestones

### M0 — Research baseline

- [x] APK/native baseline documented
- [x] engine and Lua versions identified
- [x] JNI/symbol inventory established
- [x] resource transform recovered

### M1 — Script/resource recovery

- [x] all 107 Lua payloads recoverable locally
- [x] startup graph and reachable Lua/native surface mapped
- [x] transformed PNG/HPC/CSV decoding validated
- [x] Android APK/OBB import implemented

### M2 — Modern executable skeleton

- [x] Gradle/CMake project builds
- [x] project-owned JNI/native library loads
- [x] Lua 5.2.3 executes
- [x] `armeabi-v7a` + `arm64-v8a` configured
- [x] CI build/tool validation present

### M3 — Runtime compatibility

- [x] direct `Game.StartLua` contract recreated
- [x] early filesystem/search-path compatibility recreated
- [x] critical Android launcher/lifecycle path mapped
- [x] reconstructed `AppDelegate` state implemented
- [x] bounded `HPData` reader implemented
- [x] verified `LoadGL_Scene()` parser through first object prefix implemented
- [ ] close remaining reachable startup/runtime blockers

### M4 — Boot to original UI / character selection

- [x] splash/initial-scene sequencing
- [x] `HelloWorld` → `ManagementLayer` transition
- [x] callback-driven Management routes
- [x] structured server/role models
- [x] reconstructed server-selection state
- [x] ChooseHero `PartThree()` background and `BalckCloud()` motion
- [x] recovered thunder/lightning scheduling/effect contracts
- [x] interactive ChooseHero role-item rendering
- [x] selected-role focus highlight
- [x] standalone hero save metadata reader
- [x] localized profile-label composition
- [ ] finish server-selection presentation end-to-end
- [ ] finish remaining ChooseHero/role-selection presentation and transitions
- [ ] extend verified `GameLevels` parsing to the next proven boundary
- [ ] enter the next offline scene without the original native library

### M5 — Offline gameplay

- [ ] scene loading beyond the current verified boundary
- [ ] player/enemy control
- [ ] combat
- [ ] inventory/equipment
- [ ] save/load integration
- [ ] audio/video integration
- [ ] progression/tutorials

### M6 — Modern Android release target

- [x] `arm64-v8a` build target configured
- [x] 16 KiB page-alignment/build validation in CI
- [ ] Android 15/16+ device validation
- [ ] 16 KiB runtime validation on device/emulator
- [ ] lifecycle/resume/suspend validation on target devices
- [ ] reproducible release build

## Immediate priorities

1. Finish the remaining ChooseHero profile/selection presentation details and connect them cleanly to the role-selection transition.
2. Complete server-selection rendering/input/confirm flow using the already reconstructed server-state contract.
3. Extend ChooseHero lightning/thunder/audio integration only where recovered evidence defines the original behavior.
4. Recover the next `GameSceneLayerObjectData` field width/semantics after the verified `int32`/`uint32` prefix and extend `LoadGL_Scene()` only to that next proven boundary.
5. Reach the next offline scene and identify the next concrete rendering/gameplay blocker.
6. Continue Android 15/16, `arm64-v8a` and 16 KiB runtime validation.

## Documentation

Key documents include:

- [`docs/roadmap.md`](docs/roadmap.md) — phased reconstruction plan and priorities
- [`docs/android-bootstrap.md`](docs/android-bootstrap.md) — Android/native bootstrap and lifecycle research
- [`docs/initial-ui-transition.md`](docs/initial-ui-transition.md) — recovered `HelloWorld` → `ManagementLayer` transition
- [`docs/server-selection-runtime.md`](docs/server-selection-runtime.md) — recovered `NewServerList` behavior
- [`docs/choose-hero-background.md`](docs/choose-hero-background.md) — ChooseHero background and cloud evidence/runtime boundary
- [`docs/choose-hero-thunder-scheduler.md`](docs/choose-hero-thunder-scheduler.md) — recovered thunder RNG/timing contract
- [`docs/choose-hero-role-item-compositor.md`](docs/choose-hero-role-item-compositor.md) — role-item rendering/selection visual boundary
- [`docs/choose-hero-save-metadata.md`](docs/choose-hero-save-metadata.md) — bounded standalone hero save metadata format
- [`docs/hpdata-gamelevels.md`](docs/hpdata-gamelevels.md) — `HPRange` semantics and verified scene-data parse boundary
- [`docs/jni-map.md`](docs/jni-map.md) — Java/JNI/native mapping
- [`docs/native-analysis.md`](docs/native-analysis.md) — native symbol/runtime evidence
- [`docs/resource-decoding.md`](docs/resource-decoding.md) — recovered resource transform
- [`docs/lua-dependency-map.md`](docs/lua-dependency-map.md) — script dependency graph
- [`docs/lua-native-api-map.md`](docs/lua-native-api-map.md) — script/native compatibility surface

## Contributing

Reverse-engineering findings are useful even before they become reconstructed source code. Helpful contributions include function identification, Ghidra analysis, Java/JNI mapping, Cocos2d-x matching, script/resource format research, clean-room runtime implementations, Android compatibility work and behavior comparison against the original game.

When documenting reconstructed behavior, include evidence whenever practical: binary hash/version, symbol/address, strings, xrefs, imports, call relationships or runtime observations.

Do not commit original APK/OBB files, original native game binaries, copyrighted game assets, user save files or decoded proprietary scripts. Tools, original clean-room code and reverse-engineering metadata should be sufficient for users to work from their own legally obtained copy.

## Legal notice

This is an independent preservation and reverse-engineering project and is not affiliated with or endorsed by the original developers, publishers or rights holders of Never Gone.

The repository is intended to contain **original project code, documentation and reverse-engineering metadata only**. Users are expected to provide any required original game files from their own legally obtained copy.

All trademarks and copyrighted materials belong to their respective owners.