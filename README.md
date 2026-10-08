# Never Gone Recomp

Experimental clean-room reverse-engineering and recompilation project for the Android version of **Never Gone**.

The goal is to reconstruct the original game runtime as maintainable source code for modern Android, including `arm64-v8a`, without depending on the obsolete original Android toolchain and without redistributing proprietary game binaries or assets.

> [!IMPORTANT]
> The project is **not yet a playable recompilation**. It is well beyond the research-only stage: the repository now contains a modern Android/NDK shell, project-owned native runtime code, embedded Lua 5.2.3, user-owned resource import/decoding workflows, reconstructed startup/lifecycle and login state, bounded binary-scene parsers, and an increasingly evidence-driven reconstruction of the original `ChooseHero` character-selection presentation.

## Current state

The current `main` branch includes:

- a modern Gradle/CMake Android application;
- project-owned C++/JNI runtime code;
- `armeabi-v7a` and `arm64-v8a` build targets;
- embedded **Lua 5.2.3**, matching the runtime identified in the original binary;
- a deterministic decoder for transformed Lua/PNG/HPC/CSV resources;
- Android import flows for user-owned original APK and OBB data;
- reconstructed Android → JNI → startup state and lifecycle forwarding;
- reconstructed `AppDelegate` startup-state semantics;
- recovered splash/initial-scene sequencing and scene-generation handling;
- an explicit clean-room transition for the verified `HelloWorld::createUI()` → `ManagementLayer::initLoginLayer()` boundary;
- callback-driven `ManagementLayer` routes for announcement, server selection, role selection, role creation and enter-game state;
- structured parsing of recovered server-list and role-list callback payloads into project-owned C++ models;
- reconstructed `NewServerList` selection semantics, including `LastLoginServer` preselection, the recovered tap-vs-drag threshold and one-shot enter requests;
- reconstructed Lua/native startup bindings and filesystem/search-path behavior;
- host-side startup, login-state, ChooseHero and binary-parser probes for fast compatibility iteration;
- clean-room replacements for several early Lua/native services;
- recovered offline startup routing and a visible-data path into `ChooseHero` reconstruction;
- recovered/staged TexturePacker metadata for the original ChooseHero background atlases;
- a reconstructed six-second `bejingwuyun.png` fade-in from the effective shipped `PartThree()` path;
- reconstructed `BalckCloud()` foreground-cloud composition and repeated motion, including recovered integer `CCPoint` coordinate conversion behavior;
- a clean-room `ChooseHero` thunder scheduler contract covering the recovered `lrand48()` transforms, delays, fade durations, lightning-index selection and optional thunder-sound selection;
- a bounds-checked `HPData` reader/cursor with recovered `HPRange` semantics;
- verified parsing of the `GameLevels::LoadGL_Scene()` top-level prefix, first scene header, first layer header and first object-record prefix;
- CI coverage for Python tooling, Lua compatibility, startup/lifecycle state, login routing, structured login payloads, server selection, ChooseHero background/cloud/thunder contracts, scene-data parser regressions, Android/NDK compilation and page-size validation.

The critical path is no longer bootstrap discovery. Work is focused on completing the visible offline path: connect reconstructed server/role state to rendering and input, integrate the recovered ChooseHero thunder/lightning effects with staged user-owned resources, continue role-selection presentation, extend the verified `GameLevels` object boundary and reach character selection/scene entry without loading the original `libcocos2dcpp.so`.

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
- the reachable script graph exposes more than one hundred native-shaped/API dependencies;
- the original `libcocos2dcpp.so` retains roughly **26,875 defined function symbols**;
- 22 static JNI exports have been identified and additional DEX native declarations have been mapped;
- the manifest launcher is `com.hippiegame.nevergone.TJ_P_01` and the critical launcher/native lifecycle flow is mapped;
- startup reaches `assets/Script/Game/StartLua.lua`, `Game.ClientRequire`, `ShareLogic.require`, `HelloWorld` and `ManagementLayer` through known native/script paths;
- `AppDelegate::AddAllSearchPath()` and its 59 child search directories have been recovered;
- the original startup sequence contains a verified synchronous `HelloWorld::createUI()` → `ManagementLayer::initLoginLayer()` boundary;
- recovered client callbacks route `cpp_OnGameAnnoucement`, `cpp_OnGetServerList`, `cpp_OnGetRoleList`, `cpp_OnCreateTheRole` and `cpp_OnEnterGame` into distinct reconstructed `ManagementLayer` states;
- server and role callback JSON is decoded into bounded project-owned models while retaining raw callback payloads for diagnostics;
- original `NewServerList` behavior preselects only an exact `LastLoginServer` id match, rejects row selection when vertical movement exceeds 10 pixels and confirms by calling the equivalent of `g_UILogin.EnterGameLogicServer(ip, id)`;
- the shipped `ChooseHeroBackground::createUI()` effectively dispatches the `PartThree()` path rather than the previously assumed full `PartOne → PartTow → CreateSun → PartThree` chain;
- `PartThree()` creates the dark-cloud background, six lightning sprites, six thunder sprites and ground-light effect nodes, with the background fading in over six seconds;
- `BalckCloud()` creates six foreground clouds with recovered anchors, opacity, positions and repeat periods, now represented by the project-owned compositor/timeline code;
- the original ChooseHero thunder path uses the process-global libc `lrand48()` stream and recovered transforms for 3–8 second delays, 0–0.7 second thunder fades, `[A,B,C,C,C,C]` lightning selection, lightning/ground-light fade durations and optional 7-way thunder-sound selection;
- the original 32-bit `HPRange` used by `HPData::getBytes(...)` is `{byte_offset, byte_length}`;
- `GameLevels::LoadGameLevels()` loads scene, action, global and port-node sections in a recovered fixed order;
- the verified `LoadGL_Scene()` boundary reaches the first `GameSceneLayerData` header and the immediately sequential first-object `int32`/`uint32` prefix, then stops before an unresolved-width `char*` read;
- imported `gamescene/gs_list/pvp_scene.glData` data can be probed through the reconstructed reader without exposing proprietary parsed values in diagnostics;
- major classes visible by symbol include `GameScene`, `GameSceneUI`, `MEPlayer`, `EnemyObject`, `BattleManager`, `DataManager`, `GameSaveData`, `ManagementLayer`, `ChooseHero`, `NewServerList` and others.

## Project status

| Area | Status |
| --- | --- |
| Original APK baseline / hashes | Done |
| Manifest/signing metadata baseline | Done |
| Engine / Lua runtime identification | Done |
| Native symbol classification | Done |
| Java/Android bootstrap mapping | Critical launcher/lifecycle path complete |
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
| Filesystem/search-path reconstruction | Early startup path covered |
| Reconstructed `AppDelegate` state | Implemented and covered by host tests |
| Android lifecycle forwarding | Implemented for reconstructed startup state |
| Splash/initial-scene sequencing | Implemented and generation-aware |
| `HelloWorld` → `ManagementLayer` initialization boundary | Implemented as verified semantic transition |
| Callback-driven `ManagementLayer` login routes | Implemented |
| Structured server/role callback decoding | Implemented and host-tested |
| Reconstructed `NewServerList` selection state | Implemented and host-tested |
| Visible server/role UI compositor | In progress / not yet end-to-end |
| Offline login/startup route reconstruction | In progress |
| `ChooseHero` atlas staging / TexturePacker restoration | Implemented for current recovered background subset |
| `ChooseHero` `PartThree()` background fade | Implemented |
| `ChooseHero` `BalckCloud()` compositor/timeline | Implemented and host-tested |
| `ChooseHero` thunder RNG/scheduling contract | Implemented and host-tested |
| `ChooseHero` lightning/thunder rendering + audio integration | In progress |
| Role-selection UI / character interaction | Not yet complete |
| `HPData` / `HPRange` reconstruction | Reader foundation done; range semantics verified |
| `GameLevels` binary format reconstruction | In progress; first layer + first object prefix verified |
| Imported `pvp_scene.glData` readiness probe | Implemented through current verified parser boundary |
| Rendering/input reconstruction | In progress; current critical path |
| Original login/title visual flow | Not yet reached end-to-end |
| Offline gameplay | Not yet functional |
| `arm64-v8a` build configuration | Done |
| 16 KiB page-size build validation | Covered in CI; device/runtime validation still needed |
| Android 15/16 runtime validation | Pending |
| Playable recompilation | Not yet |

## Reconstruction strategy

The project does **not** attempt a blind line-by-line rewrite of the original `libcocos2dcpp.so`.

The current strategy is dependency-driven and evidence-driven:

1. recover static metadata, symbols, DEX/JNI relationships and script dependencies;
2. decode user-owned resources locally with reproducible tooling;
3. execute the recovered startup scripts against a clean modern runtime;
4. reconstruct startup/lifecycle and UI state machines at verified semantic boundaries instead of copying the original ABI;
5. convert recovered callback payloads into bounded project-owned models before attaching them to renderer/UI state;
6. use missing globals, tracebacks, binary call ordering and behavior differences to identify the next required compatibility surface;
7. reconstruct animation/timeline behavior from verified call order, action parameters and random transforms rather than visual guesswork;
8. reconstruct binary resource readers only to verified field boundaries, assigning semantic names only when original control flow makes them unambiguous;
9. recreate only the native/game behavior required by the reachable offline runtime path;
10. reconstruct rendering, scene flow and gameplay systems incrementally;
11. preserve offline behavior while replacing or stubbing obsolete service integrations where necessary.

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
│   ├── choose-hero-thunder-scheduler.md
│   ├── filesystem-bindings.md
│   ├── hpdata-gamelevels.md
│   ├── initial-ui-transition.md
│   ├── jni-map.md
│   ├── lua-binding-call-shapes.md
│   ├── lua-dependency-map.md
│   ├── lua-native-api-map.md
│   ├── native-analysis.md
│   ├── resource-decoding.md
│   ├── resource-search-paths.md
│   ├── server-selection-runtime.md
│   ├── roadmap.md
│   └── ...
├── tools/                # decoder, state probes, symbol/Ghidra and build helpers
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
- forward Android lifecycle events into reconstructed `AppDelegate` state;
- preserve pre-resume startup phases and make surface readiness idempotent;
- maintain recovered scene/splash generation state across reloads;
- expose a tested semantic `HelloWorld` → `ManagementLayer` initial-UI transition;
- defer login callback routes until the recovered ManagementLayer boundary is active;
- retain route-scoped structured server/role models;
- maintain reconstructed server selection state and produce a one-shot enter request from the selected record;
- stage recovered user-owned resources for reconstructed routes such as `choose-role`;
- restore TexturePacker source-canvas placement for the recovered ChooseHero background subset;
- compose the current recovered `PartThree()` background fade and foreground cloud motion from imported assets;
- expose a verified scheduler contract for later ChooseHero lightning/thunder integration;
- locate imported `gamescene/gs_list/pvp_scene.glData` data and validate it through the currently verified `LoadGL_Scene` parser boundary.

It does **not** link against the original `libcocos2dcpp.so`.

See **[`android/README.md`](android/README.md)** for runtime/build details.

## Recovered login/UI flow

The reconstructed runtime models startup and login progression as explicit clean-room state rather than advancing merely because a splash timer elapsed.

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

The implementation is generation-aware: importing/reloading assets or starting a fresh recovered scene sequence resets the transition and pending routes so stale state cannot activate a new scene generation.

### Structured login payloads

Recovered callbacks are decoded into project-owned models:

- `cpp_OnGetServerList(...)` preserves server `id`, `name`, `ip`, optional battle endpoint data and the separate `LastLoginServer` argument;
- `cpp_OnGetRoleList(...)` preserves recovered role identity/selection fields from nested character records;
- raw callback payloads remain available for diagnostics, while renderer-facing state is filtered by the active Management route.

### Reconstructed server selection

The project has a host-tested semantic replacement for the recovered `NewServerList` selection state:

- preselection occurs only when `LastLoginServer` exactly matches a decoded server id;
- row taps are rejected when absolute vertical movement is greater than 10 pixels;
- selection changes are tracked independently of presentation;
- confirmation produces a one-shot enter request containing the selected server id, name and ip;
- the recovered destination contract is equivalent to `g_UILogin.EnterGameLogicServer(ip, id)`.

This does **not** mean the original `ManagementLayer`/`NewServerList` widget ABI or visual assets have been recreated. The current task is to consume these verified contracts from the project-owned renderer/compositor.

See **[`docs/initial-ui-transition.md`](docs/initial-ui-transition.md)** and **[`docs/server-selection-runtime.md`](docs/server-selection-runtime.md)**.

## Recovered Lua/native compatibility layer

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

The modern Android app also supports user-owned original resource import into app-private runtime storage, including OBB content and binary scene data needed by later reconstructed UI/gameplay paths.

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

Current regressions cover, among other things:

- reconstructed Lua startup behavior;
- `AppDelegate` startup-state semantics;
- the initial UI transition into `ManagementLayer`;
- callback-driven Management routes and scene-generation reset behavior;
- structured server/role callback payload decoding;
- reconstructed `NewServerList` selection and confirmation semantics;
- splash/local-login timing gates;
- ChooseHero background composition and source-canvas restoration;
- `BalckCloud()` repeated foreground-cloud motion and coordinate conversion;
- recovered ChooseHero thunder RNG transforms and scheduling primitives;
- bounded `HPData`/`GameLevels` parsing with truncated and hostile-length inputs.

These probes intentionally use clean-room state and synthetic data where possible so proprietary game resources do not need to live in the repository.

## Character-selection reconstruction

The project is actively reconstructing the offline `choose-role` / `ChooseHero` path.

Current verified work includes:

- recovered `ChooseHeroBackground` function/symbol evidence;
- recovered background TexturePacker atlas names and frame metadata;
- verified OBB paths for required user-supplied atlas pairs;
- app-private staging and validation of those atlas resources;
- restoration of TexturePacker source-canvas placement metadata;
- native scene-owned backing state for staged background layers;
- recovery of the effective shipped `createUI()` dispatch into `PartThree()`;
- rendering of the six-second dark-cloud background fade from `bejingwuyun.png`;
- recovery and rendering of six `BalckCloud()` foreground-cloud sprites with original z-order, anchors, opacity and repeated motion periods;
- exact project-owned handling of the integer coordinate conversion observed in the shipped cloud path;
- recovery of the ChooseHero random thunder scheduler's `lrand48()` draw/transform contract;
- host-tested planning helpers for thunder delay/fade, lightning selection, thunder sound selection and lightning/ground-light fade durations;
- recovery of the `ChooseHeroReadyUIScene::initLevelMap()` path into `GameScene::LoadGameLevelsWithFile()` for `gamescene/gs_list/pvp_scene.glData`.

Still remaining in this area:

- own/integrate runtime random-state progression without changing the recovered draw-order contract;
- stage and render the recovered `shandian%02d.png`, `menlei%02d.png` and `diguang.png` effect nodes;
- connect the seven recovered thunder audio choices to imported user-owned sound assets;
- reproduce the synchronized `ChooseHeroReadyUIScene::RandomShowwshandianEff(...)` behavior;
- complete role-selection UI and interaction;
- connect verified scene-data parsing far enough to enter the next offline scene.

The project intentionally does **not** guess unresolved animation, UI or scene-data semantics. Rendering and deeper scene parsing are connected only when sufficient binary evidence has been recovered.

See **[`docs/choose-hero-background.md`](docs/choose-hero-background.md)** and **[`docs/choose-hero-thunder-scheduler.md`](docs/choose-hero-thunder-scheduler.md)**.

## HPData / GameLevels reconstruction

The project has a clean-room foundation for the binary scene format used by `GameLevels`.

Recovered evidence establishes that the original `HPRange` passed by value into `HPData::getBytes(...)` consists of a byte offset followed by a byte length. The replacement implementation does not mirror the old ABI directly; it uses explicit bounds-checked offsets, lengths and a transactional sequential cursor.

For `GameLevels::LoadGL_Scene()` the currently verified parse boundary includes:

1. one unresolved signed 32-bit top-level value;
2. the scene count;
3. for the first scene, a byte-length field followed by one still-unresolved skipped byte;
4. the bounded string payload;
5. two floats assigned as a point;
6. the scene layer count;
7. the first layer's still-opaque float;
8. that layer's `object_count`;
9. for the first object, one immediately sequential `int32` followed by one `uint32`.

The parser stops deliberately before the next unresolved-width `char*` read in the original object-record path. Output is updated only after the entire verified boundary is available, and imported-asset diagnostics report readiness rather than proprietary parsed values.

See **[`docs/hpdata-gamelevels.md`](docs/hpdata-gamelevels.md)**.

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
- [x] manifest/signing metadata documented
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
- [x] critical Android launcher/native-loading/lifecycle path mapped
- [x] reconstructed `AppDelegate` startup/lifecycle state implemented
- [x] project-owned bounded `HPData` reader/cursor implemented
- [x] original `HPRange` offset/length semantics recovered
- [x] verified `GameLevels::LoadGL_Scene()` parser through first object prefix implemented
- [ ] complete remaining real startup/runtime compatibility blockers

### M4 — Boot to original UI / character selection

- [x] reconstruct recovered splash/initial-scene sequencing
- [x] model verified `HelloWorld::createUI()` → `ManagementLayer::initLoginLayer()` semantic boundary
- [x] gate reconstructed login route/timing on that UI transition
- [x] route recovered login callbacks through project-owned Management state
- [x] decode structured server/role callback payloads
- [x] reconstruct server selection/confirm state from `NewServerList`
- [x] reconstruct and render current verified ChooseHero `PartThree()` background subset
- [x] reconstruct `BalckCloud()` foreground-cloud motion
- [x] recover and model the ChooseHero thunder RNG/scheduler contract
- [ ] render server selection using the recovered state contract
- [ ] render role selection / role creation state
- [ ] integrate ChooseHero lightning/thunder/ground-light effects and audio
- [ ] complete rendering and input required for the visible original login/role flow
- [ ] extend verified `GameLevels` parsing beyond the first object prefix

### M5 — Offline gameplay

- [ ] character selection completion
- [ ] scene loading
- [ ] player/enemy control
- [ ] combat
- [ ] inventory/equipment
- [ ] save/load
- [ ] audio/video
- [ ] progression/tutorials

### M6 — Modern Android release target

- [x] `arm64-v8a` build target configured
- [x] 16 KiB page-alignment/build validation present in CI
- [ ] Android 15/16+ device validation
- [ ] explicit runtime validation on 16 KiB page-size devices/emulators
- [ ] full lifecycle/resume/suspend validation on target devices
- [ ] reproducible release build

## Immediate priorities

1. Integrate the recovered ChooseHero thunder scheduler with staged `shandian`, `menlei` and `diguang` sprites while preserving the verified RNG draw order and action timings.
2. Connect the recovered seven-way thunder sound selection to user-imported audio and reproduce the synchronized `RandomShowwshandianEff(...)` path.
3. Connect the structured server-list model and reconstructed server-selection state to the renderer/compositor, including touch hit-testing and confirm dispatch.
4. Extend the same project-owned visible flow through role selection, role creation and the enter-game/ChooseHero transition.
5. Recover the width/semantics of the next `GameSceneLayerObjectData` field after the verified `int32`/`uint32` prefix, then extend `LoadGL_Scene()` only to the next proven boundary.
6. Close remaining startup/runtime compatibility blockers discovered by host and Android probes.
7. Validate `arm64-v8a`, Android 15/16 and 16 KiB runtime behavior on real/emulated devices.
8. Expand native subsystem reconstruction only as the reachable offline path requires it.

## Documentation

Key documents include:

- **[`docs/roadmap.md`](docs/roadmap.md)** — phased reconstruction plan and current priorities
- **[`docs/android-bootstrap.md`](docs/android-bootstrap.md)** — Android/native bootstrap and lifecycle research
- **[`docs/initial-ui-transition.md`](docs/initial-ui-transition.md)** — recovered `HelloWorld` → `ManagementLayer` transition and renderer gate
- **[`docs/server-selection-runtime.md`](docs/server-selection-runtime.md)** — recovered `NewServerList` behavior and project-owned selection contract
- **[`docs/jni-map.md`](docs/jni-map.md)** — Java/JNI/native mapping
- **[`docs/native-analysis.md`](docs/native-analysis.md)** — native symbol/runtime evidence
- **[`docs/resource-decoding.md`](docs/resource-decoding.md)** — recovered resource transform
- **[`docs/lua-dependency-map.md`](docs/lua-dependency-map.md)** — script dependency graph
- **[`docs/lua-native-api-map.md`](docs/lua-native-api-map.md)** — script/native compatibility surface
- **[`docs/filesystem-bindings.md`](docs/filesystem-bindings.md)** — reconstructed filesystem APIs
- **[`docs/choose-hero-background.md`](docs/choose-hero-background.md)** — ChooseHero background, `PartThree()` and `BalckCloud()` evidence/runtime boundary
- **[`docs/choose-hero-thunder-scheduler.md`](docs/choose-hero-thunder-scheduler.md)** — recovered random-thunder RNG, timing and selection contract
- **[`docs/hpdata-gamelevels.md`](docs/hpdata-gamelevels.md)** — recovered `HPRange` semantics and verified `GameLevels` scene-data parsing boundary

## Contributing

Reverse-engineering findings are useful even before they become reconstructed source code. Helpful contributions include function identification, Ghidra analysis, Java/JNI mapping, Cocos2d-x matching, script/resource format research, clean-room runtime implementations, Android compatibility work and behavior comparison against the original game.

When documenting reconstructed behavior, include evidence whenever practical: binary hash/version, symbol/address, strings, xrefs, imports, call relationships or runtime observations.

Do not commit original APK/OBB files, original native game binaries, copyrighted game assets or decoded proprietary scripts. Tools, original clean-room code and reverse-engineering metadata should be sufficient for users to work from their own legally obtained copy.

## Legal notice

This is an independent preservation and reverse-engineering project and is not affiliated with or endorsed by the original developers, publishers or rights holders of Never Gone.

The repository is intended to contain **original project code, documentation and reverse-engineering metadata only**. Users are expected to provide any required original game files from their own legally obtained copy.

All trademarks and copyrighted materials belong to their respective owners.