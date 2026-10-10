# Never Gone Recomp

Experimental clean-room reverse-engineering and recompilation project for the Android version of **Never Gone**.

The goal is to reconstruct the original game runtime as maintainable source code for modern Android, including `arm64-v8a`, without depending on the obsolete original Android toolchain and without redistributing proprietary game binaries or assets.

> [!IMPORTANT]
> The project is **not yet a playable recompilation**. It is beyond the research/bootstrap stage: the repository already contains a modern Android/NDK runtime, embedded Lua 5.2.3, user-owned APK/OBB import, reconstructed startup/login/ChooseHero state, bounded GameLevels parsing, a project-owned scene construction pipeline, and the first renderer-facing GameScene asset staging path. The remaining blocker is connecting these pieces into one contiguous on-device path that visibly renders the first offline scene and then spawns a controllable player.

## Current state

As of **2026-10-10**, `main` includes:

- a modern Gradle/CMake Android application with project-owned C++/JNI code;
- `armeabi-v7a` and `arm64-v8a` build targets;
- embedded **Lua 5.2.3**, matching the runtime identified in the original binary;
- deterministic decoding for transformed Lua/PNG/HPC/CSV resources;
- Android import flows for user-owned original APK and OBB data;
- reconstructed Android → JNI → startup state and lifecycle forwarding;
- reconstructed `AppDelegate`, splash/scene-generation and `HelloWorld` → `ManagementLayer` behavior;
- structured server/role payload models and reconstructed `NewServerList` selection semantics;
- a substantial clean-room `ChooseHero` presentation: background/clouds, thunder/lightning state, role items, selection/focus behavior, save-derived hero metadata and localized profile labels;
- a bounds-checked `HPData` replacement with verified `HPRange {byte_offset, byte_length}` semantics;
- an evidence-backed parser through a **complete first GameScene object record**, including version-gated and type-specific tails;
- retained project-owned GameLevels scene/model state for the reconstructed enter-game route;
- an evidence-backed GameScene construction plan preserving original layer/object traversal order;
- recovered type-0 resource selection and sprite transforms for the first visual scene path;
- a live GameScene render queue built from the retained construction plan;
- direct imported-asset resolution for evidence-backed type-0 sprite commands;
- stable JNI snapshots of direct GameScene asset requests;
- an atomic native pixel store for decoded direct GameScene assets;
- a Java-side `BitmapFactory` stager that resolves imported PNGs beneath the app-private asset root, converts them to ARGB_8888, bounds allocations and publishes a complete revision transactionally;
- focused host/Android CI for startup/login, ChooseHero, scene parsing/construction, render-queue state, asset bridges and platform/build checks.

The newest direct-asset stager is intentionally still isolated: it is **not yet called from the GLSurfaceView frame/reload path**. Therefore the repository has renderer-ready state and asset pixels, but it does not yet claim a visibly rendered first offline GameScene on a real device.

## Delivery readiness

The primary delivery metric is [`docs/playable-path.md`](docs/playable-path.md). A checkpoint counts only when all earlier checkpoints work in the same project-owned runtime path.

| ID | Checkpoint | Current state |
| --- | --- | --- |
| P0 | Clean project-owned launch | **Done** |
| P1 | User-owned original data available | **Done** |
| P2 | Reconstructed startup/login | **Done** |
| P3 | Server selection end-to-end | In progress |
| P4 | ChooseHero end-to-end | In progress, substantial |
| P5 | Deterministic enter-game transition | In progress |
| P6 | GameLevels data sufficient for first scene | In progress; parser/model/construction work is substantial |
| P7 | First offline scene instantiated and visibly rendered | Not yet |
| P8 | Player spawned | Not yet |
| P9 | Input visibly changes player state | Not yet |
| P10 | Minimal save → restart → restore | Not yet |

By the project's strict contiguous metric, the highest completed checkpoint is currently **P2**: **3 of the 10 technical-alpha checkpoints P0-P9 are contiguous-complete (30%)**. This deliberately understates downstream engineering progress; substantial P4-P7 support already exists, but it does not count as playable-path completion until P3 and the intervening transitions are connected end-to-end.

**Technical alpha** requires P0-P9 on a real Android runtime with user-owned imported data. **Early playable alpha** additionally requires at least one enemy interaction, a damage/death loop, minimal persistence and no blocking crash in the first gameplay slice.

Tracking issue: [#151 — M4 → First Offline Scene / Technical Alpha path](https://github.com/Hanz0Hatt0r1/nevergone-recomp/issues/151).

## Immediate critical path

1. Finish visible server-selection rendering/input/confirmation and close the contiguous P3 transition.
2. Finish ChooseHero → reconstructed login Lua → enter-game as one generation-safe, one-shot P4/P5 route.
3. Remove the remaining artificial GameLevels limitations only where binary evidence supports the next fields/records.
4. Connect `GameSceneDirectAssetStager` to the GL frame/reload path and consume the live render queue/pixel store.
5. Render the first evidence-backed static GameScene layer from user-owned imported data without `libcocos2dcpp.so`.
6. Recover and spawn the minimal player visual/state, then expose one visible input path.
7. Add minimal save/restart/restore only after the first playable slice is stable.
8. Validate Android 15/16, arm64 and 16 KiB page-size behavior at runtime, not only at build time.

## Established findings

The reconstruction has established that:

- package: `com.hippiegame.nevergone`;
- original version: `1.0.9`;
- original native ABI: **`armeabi-v7a` only**;
- original native libraries: `libcocos2dcpp.so` and `libffmpeg.so`;
- the main library identifies its engine as **`cocos2d-2.1rc0-x-2.1.2`**;
- the embedded Lua runtime is **Lua 5.2.3**;
- the APK contains **107 transformed `.lua` modules**;
- the shipped `cocos2d::Decode` transform/key have been recovered and all 107 Lua payloads can be decoded locally from a user-owned APK;
- the static Lua graph contains 107 modules, 104 resolved edges and 101 modules reachable from `Game.StartLua`;
- the original `libcocos2dcpp.so` retains roughly **26,875 defined function symbols**;
- the launcher is `com.hippiegame.nevergone.TJ_P_01` and the critical Java/JNI/native lifecycle path is mapped;
- `AppDelegate::AddAllSearchPath()` and its 59 child search directories have been recovered;
- startup reaches `Game.StartLua`, `Game.ClientRequire`, `ShareLogic.require`, `HelloWorld` and `ManagementLayer` through known paths;
- original `NewServerList` preselection/tap-vs-drag/one-shot confirmation behavior is reconstructed;
- the shipped ChooseHero `PartThree()` / `BalckCloud()` path and thunder scheduling behavior are substantially reconstructed;
- the original 32-bit `HPRange` is `{byte_offset, byte_length}`;
- `GameLevels::LoadGameLevels()` loads scene, action, global and port-node sections in a recovered fixed order;
- the first GameScene object record is now parsed through its complete evidence-backed record boundary rather than only the old int32/uint32 prefix;
- the original visual scene path visits at most ordered layer slots `0..10`;
- type-0 objects now have evidence-backed resource-selection and sprite-transform contracts where proven;
- nonzero object construction, frame-cache atlas/plist loading, scene-action playback, anchor semantics and later gameplay objects remain separate unresolved boundaries.

## Project status

| Area | Status |
| --- | --- |
| APK / manifest / engine / Lua baseline | Done |
| Resource transform + Lua decoding | Done |
| Modern Gradle/CMake Android shell | Done |
| Project-owned JNI/native runtime | Done |
| Lua 5.2.3 integration | Done |
| User-owned APK/OBB import | Done |
| Startup/AppDelegate/initial UI state | Substantially implemented |
| Server-selection semantics | Implemented and host-tested |
| Visible server-selection path | In progress |
| ChooseHero presentation/state | Substantially reconstructed |
| ChooseHero → enter-game end-to-end | In progress |
| `HPData` foundation | Done for current parser needs |
| First GameScene object record | Evidence-backed complete record parser |
| Generic/full GameLevels scene parsing | In progress |
| Retained scene instance | Implemented |
| GameScene construction plan | Implemented for proven traversal/type-0 semantics |
| Live GameScene render queue | Implemented |
| Direct imported sprite path resolution | Implemented |
| Direct asset JNI request bridge | Implemented |
| Atomic decoded-pixel store | Implemented |
| Java direct-asset staging | Implemented, not yet wired into GL frame/reload |
| First visible offline scene | Not yet |
| Player spawn/input | Not yet |
| Offline combat/gameplay | Not yet functional |
| `arm64-v8a` build target | Done |
| 16 KiB build/page-alignment checks | Covered in CI |
| Android 15/16 + arm64 + 16 KiB runtime validation | Pending |
| Playable recompilation | Not yet |

## Reconstruction strategy

The project does **not** attempt a blind line-by-line rewrite of the original `libcocos2dcpp.so`.

The current strategy is dependency-driven and evidence-driven:

1. recover metadata, symbols, DEX/JNI relationships and script dependencies;
2. decode user-owned resources locally with reproducible tooling;
3. execute recovered startup scripts against a clean modern runtime;
4. reconstruct state machines at verified semantic boundaries instead of copying the old ABI;
5. convert callback/save/resource payloads into bounded project-owned models before attaching them to renderer state;
6. reconstruct binary readers only through proven field widths/order and stop when evidence runs out;
7. preserve recovered traversal, transforms and resource-selection semantics separately from still-unknown gameplay meaning;
8. build project-owned scene/runtime state from those proven models;
9. recreate only the native/game behavior required by the reachable offline path;
10. preserve offline behavior while replacing or stubbing obsolete service integrations where necessary.

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

Original Never Gone APK/OBB files, decoded proprietary scripts, saves and copyrighted assets are intentionally not stored in the repository.

## Reconstructed runtime flow

Current project-owned flow:

```text
Android launcher/lifecycle
        |
        v
reconstructed AppDelegate
        |
        v
HelloWorld::createUI()
        |
        v
ManagementLayer::initLoginLayer()
        |
        +--> announcement
        +--> server selection
        +--> role selection / ChooseHero
        +--> role-created
        `--> entering-game
                 |
                 v
          GameLevels model
                 |
                 v
        retained scene instance
                 |
                 v
       construction plan
                 |
                 v
          render queue
                 |
                 v
 direct imported asset requests
                 |
                 v
   decoded ARGB pixel store
                 |
                 `--> GL integration still pending
```

The runtime does **not** link against the original `libcocos2dcpp.so`.

## GameLevels / first-scene reconstruction

The `HPData` replacement is bounds-checked and transactional. Current GameLevels evidence covers:

- top-level format gate and scene count;
- first-scene string/point/layer-count header;
- first-layer header and object count;
- complete evidence-backed first-object record parsing;
- version-gated uint32 vector data;
- conditional object headers;
- type-tail shapes for object values `4`, `6`, `9` and `10`;
- imported `gamescene/gs_list/pvp_scene.glData` probing without logging proprietary field contents.

Renderer-facing reconstruction adds a separate semantic layer:

- original layer traversal is preserved as ordered slots `0..10`;
- type-0 visual objects carry proven resource-selection and transform information;
- special direct-file sprite names are resolved beneath the imported asset root;
- the retained scene/construction plan is transformed into a live render queue;
- Java can snapshot direct-file requests, decode the corresponding PNGs and atomically stage bounded ARGB pixels into native memory.

The next important boundary is no longer "discover the first object prefix". It is to **connect the staged asset pixels and render queue to the Android GL renderer and visibly instantiate the first offline scene**, while continuing parser generalization only where required by that real scene.

See:

- [`docs/hpdata-gamelevels.md`](docs/hpdata-gamelevels.md)
- [`docs/game-scene-object-construction.md`](docs/game-scene-object-construction.md)
- [`docs/playable-path.md`](docs/playable-path.md)

## Resource recovery

Decode supported transformed assets locally from a legally obtained APK:

```bash
python3 tools/asset_decoder.py /path/to/com.hippiegame.nevergone.apk \
  --output build/decoded-assets
```

Recovered proprietary output should remain local and must not be committed.

The Android app also supports user-owned original APK/OBB import into app-private runtime storage.

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

## Testing and CI

The repository uses synthetic fixtures and host probes wherever possible so proprietary resources do not need to live in source control.

Coverage includes:

- Lua/startup compatibility;
- AppDelegate/lifecycle and initial UI state;
- Management/server/role state;
- ChooseHero visuals, selection and metadata;
- HPData/GameLevels parsing and failure bounds;
- retained GameLevels model/scene state;
- GameScene construction-plan semantics;
- render-queue ordering/state;
- direct-asset path resolution;
- JNI request snapshots;
- atomic direct-asset pixel-store behavior;
- Android Java compilation;
- NDK/build and page-size validation.

Full validation requiring a user-owned original APK remains manual-only; ordinary PR checks are designed to stay reproducible without proprietary inputs.

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
- [x] focused CI validation present

### M3 — Runtime compatibility

- [x] direct `Game.StartLua` contract recreated
- [x] early filesystem/search-path compatibility recreated
- [x] reconstructed `AppDelegate` state implemented
- [x] bounded HPData/GameLevels foundation implemented
- [ ] close remaining contiguous login/runtime blockers

### M4 — First offline scene / technical-alpha path

- [x] splash/initial-scene sequencing
- [x] `HelloWorld` → `ManagementLayer`
- [x] Management server/role models and callbacks
- [x] substantial ChooseHero presentation/state reconstruction
- [x] complete first-object record parser
- [x] retained scene instance + construction plan
- [x] live render queue
- [x] direct imported asset request/pixel staging pipeline
- [ ] finish server-selection end-to-end
- [ ] finish ChooseHero → enter-game end-to-end
- [ ] connect direct asset staging to GL frame/reload
- [ ] visibly render first offline scene
- [ ] spawn player
- [ ] expose visible player input

### M5 — Offline gameplay

- [ ] first enemy interaction
- [ ] damage/death loop
- [ ] combat systems
- [ ] inventory/equipment
- [ ] minimal save/restart/restore
- [ ] audio/video integration
- [ ] progression/tutorials

### M6 — Modern Android release target

- [x] `arm64-v8a` build target configured
- [x] 16 KiB page-alignment/build validation in CI
- [ ] Android 15/16 device/emulator validation
- [ ] arm64 runtime validation
- [ ] 16 KiB runtime validation
- [ ] lifecycle/resume/surface-recreation validation
- [ ] reproducible release build

## Documentation

Key documents:

- [`docs/playable-path.md`](docs/playable-path.md) — primary delivery metric and P0-P10 gates
- [`docs/roadmap.md`](docs/roadmap.md) — phased reconstruction plan
- [`docs/hpdata-gamelevels.md`](docs/hpdata-gamelevels.md) — HPRange and binary scene parsing evidence
- [`docs/game-scene-object-construction.md`](docs/game-scene-object-construction.md) — layer traversal, type-0 construction/resource/transform evidence
- [`docs/android-bootstrap.md`](docs/android-bootstrap.md) — Android/native bootstrap and lifecycle research
- [`docs/initial-ui-transition.md`](docs/initial-ui-transition.md) — `HelloWorld` → `ManagementLayer`
- [`docs/server-selection-runtime.md`](docs/server-selection-runtime.md) — `NewServerList` behavior
- [`docs/choose-hero-background.md`](docs/choose-hero-background.md) — ChooseHero background/cloud path
- [`docs/choose-hero-thunder-scheduler.md`](docs/choose-hero-thunder-scheduler.md) — thunder RNG/timing contract
- [`docs/choose-hero-role-item-compositor.md`](docs/choose-hero-role-item-compositor.md) — role-item rendering/selection boundary
- [`docs/choose-hero-save-metadata.md`](docs/choose-hero-save-metadata.md) — bounded hero save metadata
- [`docs/jni-map.md`](docs/jni-map.md) — Java/JNI/native mapping
- [`docs/native-analysis.md`](docs/native-analysis.md) — native symbol/runtime evidence
- [`docs/resource-decoding.md`](docs/resource-decoding.md) — recovered resource transform
- [`docs/lua-dependency-map.md`](docs/lua-dependency-map.md) — script dependency graph
- [`docs/lua-native-api-map.md`](docs/lua-native-api-map.md) — Lua/native compatibility surface

## Contributing

Reverse-engineering findings are useful even before they become reconstructed source code. Helpful contributions include function identification, Ghidra analysis, Java/JNI mapping, Cocos2d-x matching, script/resource format research, clean-room runtime implementations, Android compatibility work and behavior comparison against the original game.

When documenting reconstructed behavior, include evidence whenever practical: binary hash/version, symbol/address, strings, xrefs, imports, call relationships or runtime observations.

Do not commit original APK/OBB files, original native game binaries, copyrighted game assets, user saves or decoded proprietary scripts. Tools, original clean-room code and reverse-engineering metadata should be sufficient for users to work from their own legally obtained copy.

## Legal notice

This is an independent preservation and reverse-engineering project and is not affiliated with or endorsed by the original developers, publishers or rights holders of Never Gone.

The repository is intended to contain **original project code, documentation and reverse-engineering metadata only**. Users are expected to provide any required original game files from their own legally obtained copy.

All trademarks and copyrighted materials belong to their respective owners.
