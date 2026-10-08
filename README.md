# Never Gone Recomp

Experimental clean-room reverse-engineering and recompilation project for the Android version of **Never Gone**.

The project aims to reconstruct the original game runtime as maintainable source code for modern Android, including `arm64-v8a`, without depending on the original obsolete Android toolchain or redistributing proprietary game binaries/assets.

> [!IMPORTANT]
> The project is **not yet a playable recompilation**, but it has moved well beyond the initial research-only stage. A modern Android/NDK shell now builds project-owned native code, embeds the identified Lua runtime, imports user-owned original resources, executes reconstructed startup behavior, stages recovered character-selection resources, and probes verified portions of the original binary scene-data format through clean-room parsers.

## Current state

The current `main` branch includes:

- a modern Gradle/CMake Android application;
- project-owned C++/JNI runtime code;
- `armeabi-v7a` and `arm64-v8a` build targets;
- embedded **Lua 5.2.3**, matching the runtime identified in the original binary;
- a deterministic decoder for the original transformed Lua/PNG/HPC/CSV resources;
- Android import flows for user-owned original APK and OBB data;
- reconstructed startup Lua/native bindings and filesystem/search-path behavior;
- host-side startup probing for fast compatibility iteration;
- clean-room replacements for several early Lua/native services;
- recovered offline startup routing and early `ChooseHero` reconstruction/staging work;
- a bounds-checked clean-room `HPData` reader/cursor and recovered `HPRange` semantics;
- verified parsing/probing of the `GameLevels::LoadGL_Scene()` top-level prefix and first scene header from imported user-owned data;
- CI covering Python tooling, Lua compatibility, scene-data parser regressions and Android/NDK compilation.

The current implementation target is the original initial UI/runtime flow, character-selection path and the minimum scene-data/runtime substrate needed to reach offline gameplay without loading the original `libcocos2dcpp.so`.

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
- the exact `cocos2d::Decode` algorithm and key used by the shipped build have been recovered;
- all 107 Lua payloads can be decoded locally from a user-owned APK;
- the static Lua graph contains 107 modules, 104 resolved edges and 101 modules reachable from `Game.StartLua`;
- the reachable script graph exposes more than one hundred native-shaped/API dependencies, allowing reconstruction to proceed in dependency order;
- the original `libcocos2dcpp.so` retains roughly **26,875 defined function symbols**, including many useful game class/function names;
- 22 static JNI exports have been identified and additional DEX native declarations have been mapped;
- startup reaches `assets/Script/Game/StartLua.lua`, `Game.ClientRequire`, `ShareLogic.require`, `HelloWorld` and `ManagementLayer` through known native/script paths;
- `AppDelegate::AddAllSearchPath()` and its 59 child search directories have been recovered;
- the original 32-bit `HPRange` used by `HPData::getBytes(...)` is `{byte_offset, byte_length}`;
- `GameLevels::LoadGameLevels()` loads scene, action, global and port-node sections in a recovered fixed order;
- the beginning of `GameLevels::LoadGL_Scene()` has a verified top-level scene count and a verified first-scene header containing a bounded string payload, two floats used as a point and a layer count;
- the imported `gamescene/gs_list/pvp_scene.glData` resource can be probed through the reconstructed reader without exposing proprietary parsed values in diagnostics;
- major classes visible by symbol include `GameScene`, `GameSceneUI`, `MEPlayer`, `EnemyObject`, `BattleManager`, `DataManager`, `GameSaveData`, `ManagementLayer`, `ChooseHero` and others.

## Project status

| Area | Status |
| --- | --- |
| Original APK baseline / hashes | Done |
| Modern Android installation experiment | Done |
| Engine / Lua runtime identification | Done |
| Native symbol classification | Done |
| JNI/startup mapping | Substantially mapped |
| Asset/Lua transform recovery | Done |
| Lua decoder / recovered script workflow | Done |
| Lua dependency/native API mapping | Done |
| Modern Gradle/CMake app | Done |
| Project-owned JNI/native runtime | Done |
| Lua 5.2.3 integration | Done |
| APK asset import | Done |
| OBB asset import | Done |
| Startup compatibility bindings | In progress, substantial coverage |
| Filesystem/search-path reconstruction | In progress, early startup covered |
| Offline login/startup route reconstruction | In progress |
| `ChooseHero` asset/background staging | In progress |
| `HPData` / `HPRange` reconstruction | Reader foundation done; range semantics verified |
| `GameLevels` binary format reconstruction | In progress; top-level prefix and first scene header verified |
| Imported `pvp_scene.glData` readiness probe | Done for the currently verified scene header boundary |
| Rendering/input reconstruction | In progress / next critical path |
| Original title/menu visual flow | Not yet reached end-to-end |
| Offline gameplay | Not yet functional |
| `arm64-v8a` build configuration | Done |
| Android 15/16 runtime validation | Pending |
| 16 KiB page-size validation | Pending |
| Playable recompilation | Not yet |

## Reconstruction strategy

The project does **not** attempt a blind line-by-line rewrite of the original `libcocos2dcpp.so`.

The current strategy is dependency-driven and evidence-driven:

1. recover static metadata, symbols, DEX/JNI relationships and script dependencies;
2. decode user-owned resources locally with reproducible tooling;
3. execute the real recovered startup scripts against a clean modern runtime;
4. use missing globals, tracebacks, binary call ordering and behavior differences to identify the next required compatibility surface;
5. reconstruct binary resource readers only to verified field boundaries, assigning semantic names only when original control flow makes them unambiguous;
6. recreate only the native/game behavior required by the reachable runtime path;
7. reconstruct rendering, scene flow and gameplay systems incrementally;
8. preserve offline behavior while replacing or stubbing obsolete service integrations where necessary.

See **[`docs/roadmap.md`](docs/roadmap.md)** for the detailed development plan.

## Repository layout

```text
nevergone-recomp/
├── README.md
├── android/              # modern Gradle/NDK application and runtime shell
│   ├── app/
│   └── README.md
├── docs/                 # reverse-engineering evidence and reconstruction notes
│   ├── android-bootstrap.md
│   ├── apk-analysis.md
│   ├── choose-hero-background.md
│   ├── filesystem-bindings.md
│   ├── hpdata-gamelevels.md
│   ├── jni-map.md
│   ├── lua-binding-call-shapes.md
│   ├── lua-dependency-map.md
│   ├── lua-native-api-map.md
│   ├── native-analysis.md
│   ├── resource-decoding.md
│   ├── resource-search-paths.md
│   ├── roadmap.md
│   └── ...
├── tools/                # decoder, probes, symbol/Ghidra and build helpers
├── third_party/_local/   # ignored locally fetched dependencies
└── .github/              # CI/workflows
```

Original Never Gone APK/OBB files, decoded proprietary scripts and copyrighted assets are intentionally not stored in the repository.

## Modern Android runtime

The clean-room Android shell under [`android/`](android/) can already:

- target a current Android SDK;
- build/load project-owned native C++ code;
- build for `armeabi-v7a` and `arm64-v8a`;
- report ABI/pointer width/runtime page size;
- maintain an app-local runtime root and privacy-safe persistent UUID;
- embed and execute Lua 5.2.3;
- register reconstructed startup globals;
- load decoded/imported Lua modules from app-private storage;
- expose Lua/native startup errors as diagnostics;
- import original resources supplied by the user through Android storage flows;
- stage selected recovered resources for reconstructed routes such as `choose-role`;
- locate imported `gamescene/gs_list/pvp_scene.glData` data and validate its currently verified `LoadGL_Scene` prefix/first-scene header through the project-owned bounded reader.

It does **not** link against the original `libcocos2dcpp.so`.

See **[`android/README.md`](android/README.md)** for runtime/build details.

## Recovered startup compatibility layer

The modern runtime recreates the direct Lua-visible startup contract, including semantic equivalents for:

```text
CAddDoString
Lua_GetPlatformString
Lua_GetDeviceUUID
cpp_ShowLoadingUI
cpp_HideLoadingUI
cpp_ShowErrorDialogUI
cpp_ShowMessageBoxUI
```

The reconstruction also covers multiple early reachable dependencies, including:

- Lua module loading and compatibility helpers;
- filesystem/search-path helpers;
- Lua 5.1 compatibility surfaces used by original scripts;
- observed legacy `bit` functionality through Lua 5.2 `bit32`;
- JSON encode/decode;
- LuaXML/config helpers;
- boot-safe offline `ProtoRPC` initialization;
- UUID/version/string-validation helpers;
- diagnostic bridging for early login/native callbacks.

The objective is semantic compatibility with the scripts, not ABI compatibility with the old ARM binary.

## Resource recovery

The original resource transform has been recovered and automated.

Decode supported transformed assets locally from a legally obtained APK:

```bash
python3 tools/asset_decoder.py /path/to/com.hippiegame.nevergone.apk \
  --output build/decoded-assets
```

Recovered proprietary output should remain local and must not be committed.

The modern Android app also supports user-owned original resource import into app-private runtime storage. Recent work extends this to OBB content and binary scene data needed by later reconstructed UI/gameplay paths.

## Lua 5.2.3

The repository does not vendor the upstream Lua source directly. Fetch and verify the exact runtime release with:

```bash
python3 tools/fetch_lua_5_2_3.py
```

This creates the ignored local directory:

```text
third_party/_local/lua-5.2.3/
```

The helper verifies the pinned release before extraction, and CMake integrates it into the modern runtime.

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

## Host-side probing

Host-side probes shorten the reconstruction loop without repeatedly installing an APK.

The Lua startup probe executes the recovered script chain against clean-room bindings and turns the first missing global, incompatible call shape or resource lookup into the next concrete compatibility target.

Scene-data host regressions exercise the project-owned `HPData`/`GameLevels` parsers against synthetic bounded inputs, including truncated and hostile-length cases, without requiring proprietary game data in the repository.

See the tooling documentation under [`tools/`](tools/) and the roadmap for the current probe workflow.

## Character-selection reconstruction

The project has begun reconstructing the offline `choose-role` / `ChooseHero` path.

Current work includes:

- recovered `ChooseHeroBackground` function/symbol evidence;
- recovered background TexturePacker atlas names and frame metadata;
- verified OBB paths for required user-supplied atlas pairs;
- app-private staging and validation of those atlas resources;
- restoration of TexturePacker source-canvas placement metadata;
- native scene-owned backing state for the staged background layers;
- recovery of the `ChooseHeroReadyUIScene::initLevelMap()` path into `GameScene::LoadGameLevelsWithFile()` for `gamescene/gs_list/pvp_scene.glData`.

The project intentionally does **not** guess unresolved animation/phase behavior or scene-data field semantics. Rendering and deeper scene parsing are connected only when sufficient binary evidence has been recovered.

See **[`docs/choose-hero-background.md`](docs/choose-hero-background.md)**.

## HPData / GameLevels reconstruction

The project now has a clean-room foundation for the binary scene format used by `GameLevels`.

Recovered evidence establishes that the original `HPRange` passed by value into `HPData::getBytes(...)` consists of a byte offset followed by a byte length. The replacement implementation does not mirror the old ABI directly; it uses explicit bounds-checked offsets, lengths and a transactional sequential cursor.

For `GameLevels::LoadGL_Scene()` the currently verified parse boundary includes:

1. one unresolved signed 32-bit top-level value;
2. the scene count;
3. for the first scene, a byte-length field followed by one still-unresolved skipped byte;
4. the bounded string payload;
5. two floats assigned as a point;
6. the scene layer count.

The parser updates output only after the entire verified boundary is available. Imported asset diagnostics report readiness and verified byte counts, not proprietary parsed field values.

The next known binary boundary begins inside each `GameSceneLayerData` record: a float is followed by a `uint32` used as the object-loop bound. Reconstruction will continue from that point before deeper object/action/global/port-node semantics are assigned.

See **[`docs/hpdata-gamelevels.md`](docs/hpdata-gamelevels.md)** for the current evidence and parser boundary.

## Original architecture

Current evidence supports approximately this runtime structure:

```text
Android application
│
├── classes.dex
│   ├── com.hippiegame.nevergone.*
│   ├── org.cocos2dx.lib.*
│   └── legacy Google Play / IAP integration
│
├── JNI
│   └── libcocos2dcpp.so
│       ├── Never Gone C++ code
│       ├── Cocos2d-x 2.1.2-era engine
│       ├── Lua 5.2.3 / bindings
│       ├── protobuf
│       ├── TinyXML
│       ├── resource / save / networking systems
│       └── UI / combat / scene code
│
├── libffmpeg.so
│
└── assets / OBB
    ├── transformed Lua scripts
    ├── PNG / plist UI and graphics data
    ├── HPC / CSV configuration data
    ├── binary scene/level data
    └── audio / video
```

## Major milestones

### M0 — Research baseline

- [x] APK/native baseline documented
- [x] engine and Lua versions identified
- [x] JNI/symbol inventory established
- [x] resource transform recovered

### M1 — Script/resource recovery

- [x] all 107 Lua payloads recoverable locally
- [x] startup module graph built
- [x] reachable Lua/native API surface mapped
- [x] transformed PNG/HPC/CSV decoding validated
- [x] Android user-owned APK import implemented
- [x] Android user-owned OBB import implemented

### M2 — Modern executable skeleton

- [x] Gradle/CMake project builds
- [x] project-owned JNI/native library loads
- [x] Lua 5.2.3 executes in the clean runtime
- [x] startup diagnostics available
- [x] `armeabi-v7a` + `arm64-v8a` configured
- [x] CI build/tool validation present

### M3 — Runtime compatibility

- [x] direct `Game.StartLua` contract recreated
- [x] early filesystem/search-path compatibility recreated
- [x] multiple reachable Lua/native services recreated
- [x] host-side startup probe available
- [x] project-owned bounded `HPData` reader/cursor implemented
- [x] original `HPRange` offset/length semantics recovered
- [x] first verified `GameLevels::LoadGL_Scene()` scene header parser/probe implemented
- [ ] complete real startup chain without unresolved runtime blockers
- [ ] complete AppDelegate/lifecycle reconstruction

### M4 — Boot to original UI

- [ ] initialize reconstructed rendering/input path
- [ ] reproduce `HelloWorld` → `ManagementLayer`
- [ ] connect reconstructed login/startup callbacks to UI
- [ ] render the recovered `ChooseHero` path
- [ ] extend verified `GameLevels` parsing through the scene/layer records needed by character selection

### M5 — Offline gameplay

- [ ] character selection
- [ ] scene loading
- [ ] player/enemy control
- [ ] combat
- [ ] inventory/equipment
- [ ] save/load
- [ ] audio/video
- [ ] progression/tutorials

### M6 — Modern Android release target

- [x] `arm64-v8a` build target configured
- [ ] Android 15/16+ device validation
- [ ] explicit 16 KiB page-size validation
- [ ] lifecycle/resume/suspend validation
- [ ] reproducible release build

## Immediate priorities

1. Extend evidence-driven `GameLevels::LoadGL_Scene()` reconstruction into the first layer/object records, starting from the verified layer float and object-loop count boundary.
2. Complete the remaining startup/runtime compatibility blockers using the host and Android probes.
3. Reconstruct the minimum `AppDelegate`, lifecycle, rendering and input path required for visible original UI.
4. Connect captured offline login/server/role callbacks to the reconstructed UI layer.
5. Continue evidence-driven `ChooseHero` reconstruction and connect the verified staged resources/scene data to rendering only when their semantics are established.
6. Finish the Java/Android bootstrap/lifecycle map.
7. Validate `arm64-v8a`, Android 15/16 and 16 KiB page-size behavior on real/emulated devices.
8. Expand native subsystem reconstruction only as the reachable offline path requires it.

## Documentation

Key documents include:

- **[`docs/roadmap.md`](docs/roadmap.md)** — phased reconstruction plan and current priorities
- **[`docs/android-bootstrap.md`](docs/android-bootstrap.md)** — Android/native bootstrap research
- **[`docs/jni-map.md`](docs/jni-map.md)** — Java/JNI/native mapping
- **[`docs/native-analysis.md`](docs/native-analysis.md)** — native symbol/runtime evidence
- **[`docs/resource-decoding.md`](docs/resource-decoding.md)** — recovered resource transform
- **[`docs/lua-dependency-map.md`](docs/lua-dependency-map.md)** — script dependency graph
- **[`docs/lua-native-api-map.md`](docs/lua-native-api-map.md)** — script/native compatibility surface
- **[`docs/filesystem-bindings.md`](docs/filesystem-bindings.md)** — reconstructed filesystem APIs
- **[`docs/choose-hero-background.md`](docs/choose-hero-background.md)** — character-selection background evidence/staging
- **[`docs/hpdata-gamelevels.md`](docs/hpdata-gamelevels.md)** — recovered `HPRange` semantics and verified `GameLevels` scene-data parsing boundary

## Contributing

Reverse-engineering findings are useful even before they become reconstructed source code. Helpful contributions include function identification, Ghidra analysis, Java/JNI mapping, Cocos2d-x matching, script/resource format research, clean-room runtime implementations, Android compatibility work and behavior comparison against the original game.

When documenting reconstructed behavior, include evidence whenever practical: binary hash/version, symbol/address, strings, xrefs, imports, call relationships or runtime observations.

Do not commit original APK/OBB files, original native game binaries, copyrighted game assets or decoded proprietary scripts. Tools, original clean-room code and reverse-engineering metadata should be sufficient for users to work from their own legally obtained copy.

## Legal notice

This is an independent preservation and reverse-engineering project and is not affiliated with or endorsed by the original developers, publishers or rights holders of Never Gone.

The repository is intended to contain **original project code, documentation and reverse-engineering metadata only**. Users are expected to provide any required original game files from their own legally obtained copy.

All trademarks and copyrighted materials belong to their respective owners.
