# Never Gone Recomp

Experimental clean-room reverse-engineering and recompilation project for the Android version of **Never Gone**.

The goal is to reconstruct the original game runtime as maintainable source code for modern Android, including `arm64-v8a`, without depending on the obsolete original Android toolchain and without redistributing proprietary game binaries or assets.

> [!IMPORTANT]
> The project is **not yet a playable recompilation**. It is well beyond bootstrap/research: the repository now has a modern Android/NDK runtime, embedded Lua 5.2.3, user-owned APK/OBB import, reconstructed startup/login/ChooseHero state, bounded GameLevels parsing, retained scene state, a live GameScene render queue, GLES2 texture upload and direct-sprite drawing, atlas/plist discovery and texture resolution, plus bounded native evidence for save/combat-related systems. The main blocker is no longer basic rendering infrastructure; it is closing the **contiguous on-device path** from server selection through ChooseHero/enter-game into a first scene whose default sprite-frame resources, player spawn and input all work together.

## Current state

As of **2026-10-10**, `main` includes:

- a modern Gradle/CMake Android application with project-owned C++/JNI code;
- `armeabi-v7a` and `arm64-v8a` build targets;
- embedded **Lua 5.2.3**, matching the runtime identified in the original binary;
- deterministic decoding for transformed Lua/PNG/HPC/CSV resources;
- Android import flows for user-owned original APK and OBB data;
- reconstructed Android -> JNI -> startup state and lifecycle forwarding;
- reconstructed `AppDelegate`, splash/scene-generation and `HelloWorld` -> `ManagementLayer` behavior;
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
- Java-side `BitmapFactory` staging of imported direct PNGs to bounded ARGB_8888 revisions;
- transactional upload of staged direct pixels into GLES2 texture handles;
- live direct-file GameScene sprite drawing from the renderer after frame clear;
- evidence-backed direct-sprite geometry using the original 1136x640 ExactFit design space, centered sprite anchor and clockwise Cocos rotation convention;
- discovery of the ordered GameScene atlas preload list used by the original runtime;
- sprite-frame asset request snapshots for `spriteFrameByName` paths;
- safe imported plist/atlas texture resolution, including the recovered direct `metadata.textureFileName` rule and sibling `.png` fallback;
- a reconstructed project-owned `GameSaveData::Encode/Decode` leaf codec validated against bounded unidbg probes;
- a bounded `EnemyActionsData` layout/dependency evidence map and a transactional project-owned parser for the recovered WBG Sections A-G (header, primary records, compact/nested/gated groups, fixed-tail records, combo tuples and final table);
- a repaired unidbg evidence harness that can load the original native dependency set far enough to complete `JNI_OnLoad` and run isolated synthetic native probes;
- focused host/Android CI for startup/login, ChooseHero, scene parsing/construction, render state, asset bridges, GameSaveData transforms, EnemyActions WBG structural parsing, native evidence contracts and platform/build checks.

The renderer can now submit **direct-file** GameScene sprites with reconstructed geometry. This does **not** yet mean P7 is complete: most ordinary type-0 objects use `CCSpriteFrameCache::spriteFrameByName()`, so frame metadata/extraction from the resolved imported atlases still has to be connected to rendering, and the complete first offline scene still requires real-device validation through the contiguous runtime path.

Likewise, the newly composed EnemyActions WBG parser is **structural parsing**, not combat: it has synthetic test coverage, but genuine user-owned WBG payload validation, action interpretation, enemy instantiation and combat behavior are still outstanding.

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
| P7 | First offline scene instantiated and visibly rendered | In progress; direct-file draw works, frame-cache path/device proof remain |
| P8 | Player spawned | Not yet |
| P9 | Input visibly changes player state | Not yet |
| P10 | Minimal save -> restart -> restore | Not yet; leaf save codec recovered |

By the project's strict contiguous metric, the highest completed checkpoint is still **P2**: **3 of the 10 technical-alpha checkpoints P0-P9 are contiguous-complete (30%)**. That intentionally understates downstream engineering: P6/P7 have significant implementation behind them, but none of it advances the strict marker until P3-P5 are closed in the same on-device route.

For planning, a second metric is useful: **engineering readiness toward technical alpha is approximately 55%**. This is not a release claim; it reflects how much enabling infrastructure already exists outside the contiguous checkpoint chain. The largest remaining concentration of risk is now in runtime integration, sprite-frame atlas consumption, player creation/input and device validation rather than in project/bootstrap tooling.

**Technical alpha** requires P0-P9 on a real Android runtime with user-owned imported data. **Early playable alpha** additionally requires at least one enemy interaction, a damage/death loop, minimal persistence and no blocking crash in the first gameplay slice.

Tracking issue: [#151 — M4 -> First Offline Scene / Technical Alpha path](https://github.com/Hanz0Hatt0r1/nevergone-recomp/issues/151).

## Readiness by area

| Area | Estimated readiness | Notes |
| --- | ---: | --- |
| Build/tooling/clean runtime | 90% | Modern app, JNI/native runtime, Lua, dual ABI and broad CI exist |
| Original-data import/resource recovery | 90% | APK/OBB import and core resource transforms are established |
| Startup/login/management UI | 75% | Substantial reconstruction; contiguous P3 transition still open |
| ChooseHero/enter-game route | 65% | Presentation/state are strong; end-to-end one-shot route remains open |
| GameLevels/scene model | 65% | First object boundary and retained model are strong; full first-scene sufficiency is not closed |
| First-scene rendering | 65% | Direct sprites render; atlas/frame-cache sprite extraction and device proof remain |
| Player spawn/input | 15% | Evidence exists around downstream systems, but P8/P9 are not implemented end-to-end |
| Offline combat/gameplay | 10% | Bounded EnemyActions WBG A-G structural parser exists, but real-payload validation and any playable enemy/combat loop remain open |
| Persistence | 25% | `GameSaveData` leaf codec is recovered; full save/write/restart/restore is not |
| Modern Android runtime validation | 25% | Build/page checks exist; Android 15/16, arm64 and 16 KiB runtime proof is pending |

These percentages are engineering estimates, not compatibility guarantees. The authoritative delivery gate remains the contiguous P0-P10 path.

## Immediate critical path

1. Finish visible server-selection rendering/input/confirmation and close contiguous P3.
2. Finish ChooseHero -> reconstructed login Lua -> enter-game as one generation-safe, one-shot P4/P5 route.
3. Generalize GameLevels only as far as the real first scene requires and only where binary evidence supports the next fields/records.
4. Parse resolved TexturePacker plist frame metadata and connect `spriteFrameByName` requests to atlas-backed textures/quads.
5. Prove the complete first offline GameScene on a real Android device using user-owned imported data.
6. Recover and spawn the minimal player visual/state, then expose one visible input path.
7. Validate the recovered EnemyActions WBG A-G parser against user-owned files and connect it to actual enemy/action consumers only when the first gameplay slice needs it; avoid speculative combat semantics.
8. Build minimal save/restart/restore on top of the recovered GameSaveData codec after the first playable slice is stable.
9. Validate Android 15/16, arm64 and 16 KiB page-size behavior at runtime, not only at build time.

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
- the first GameScene object record is parsed through its complete current evidence-backed record boundary rather than only the old int32/uint32 prefix;
- the original visual scene path visits at most ordered layer slots `0..10`;
- type-0 objects have evidence-backed resource-selection and sprite-transform contracts where proven;
- the original design resolution path is 1136x640 with Cocos ExactFit behavior; direct-file sprite rendering now follows that mapping;
- ordinary sprite anchor behavior is centered at `(0.5, 0.5)` and positive Cocos rotation is clockwise in the recovered path;
- direct-file GameScene pixels can be staged, uploaded to GLES2 and drawn from the live render queue without linking the original native library;
- the original GameScene atlas preload list can be discovered from imported data;
- recovered plist loading uses direct top-level `metadata.textureFileName` when present and falls back to a sibling `.png` name when absent/empty;
- `GameSaveData::Encode/Decode` use the recovered byte transform with a counter cycling `0..126`; the project-owned implementation is regression-tested against native probes;
- bounded unidbg work validated the existing asset decoder and selected Cocos geometry helpers on synthetic inputs after loading 48 runtime modules with zero unresolved symbols and completing explicit `JNI_OnLoad`;
- `EnemyActionsData::loadWBGFile` is confirmed as an HPData-backed parser dependency; its constructor/layout and Sections A-G stream topology are now recorded and parsed structurally with bounded, transactional project-owned readers;
- the recovered WBG structure has a 16-byte header, a primary counted record stream, compact/nested and header-gated groups, fixed-tail records, 24-byte combo tuples and a final int table; field semantics and real-file compatibility are not yet proven;
- nonzero GameScene object construction, atlas frame extraction/TexturePacker semantics, scene-action playback, player creation, combat and full persistence remain unresolved or incomplete boundaries.

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
| ChooseHero -> enter-game end-to-end | In progress |
| `HPData` foundation | Done for current parser needs |
| First GameScene object record | Evidence-backed complete current record parser |
| Generic/full GameLevels scene parsing | In progress |
| Retained scene instance | Implemented |
| GameScene construction plan | Implemented for proven traversal/type-0 semantics |
| Live GameScene render queue | Implemented |
| Direct imported sprite path resolution | Implemented |
| Direct asset JNI request bridge | Implemented |
| Atomic decoded-pixel store | Implemented |
| Java direct-asset staging | Implemented |
| Direct GLES2 texture upload/cache | Implemented |
| Direct GameScene sprite renderer | Implemented for proven direct-file type-0 branches |
| ExactFit/anchor/rotation geometry | Implemented for direct sprites |
| GameScene atlas preload discovery | Implemented |
| Sprite-frame request snapshot | Implemented |
| Imported atlas plist/texture resolution | Implemented |
| Atlas frame metadata -> rendered sprite | Next renderer blocker |
| First complete visible offline scene | Not yet device-proven |
| `GameSaveData` Encode/Decode leaf codec | Implemented from native probe evidence |
| Minimal save/restart/restore | Not yet |
| `EnemyActionsData` layout/dependency evidence | Implemented as bounded evidence contract |
| EnemyActions WBG Sections A-G parser | Implemented for recovered stream topology; synthetic host smoke, no genuine-file or gameplay proof |
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
7. use bounded native execution (including unidbg probes) to validate isolated leaf behavior where static analysis alone is insufficient;
8. preserve recovered traversal, transforms and resource-selection semantics separately from still-unknown gameplay meaning;
9. build project-owned scene/runtime state from those proven models;
10. recreate only the native/game behavior required by the reachable offline path;
11. preserve offline behavior while replacing or stubbing obsolete service integrations where necessary.

## Repository layout

```text
nevergone-recomp/
├── README.md
├── android/              # modern Gradle/NDK app and clean-room runtime
│   ├── app/
│   └── README.md
├── docs/                 # reverse-engineering evidence and reconstruction notes
│   └── evidence/         # bounded reproducible native/static evidence
├── tools/                # decoders, unidbg probes, Ghidra helpers and validation tools
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
           /          \
          v            v
 direct-file requests  sprite-frame requests
          |            |
          v            v
   ARGB pixel store    atlas-list/plist/texture resolution
          |            |
          v            `--> frame metadata/render hookup pending
   GLES2 texture cache
          |
          v
 direct sprite renderer
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
- Java snapshots direct-file requests, decodes corresponding imported PNGs and atomically stages bounded ARGB pixels;
- native code synchronizes complete staged revisions into GLES2 texture handles;
- the direct renderer draws validated direct-file sprites using recovered ExactFit/anchor/rotation semantics;
- default `spriteFrameByName` objects expose ordered frame requests;
- the imported GameScene atlas list, plist paths and backing texture paths can now be resolved without guessing resource names.

The next visual boundary is to **parse the resolved atlas frame dictionaries and reproduce the TexturePacker frame/source-size/offset semantics needed by the live `spriteFrameByName` requests**. After that, the first scene must be proven through the real P3-P7 device route rather than only by isolated renderer/runtime tests.

See:

- [`docs/hpdata-gamelevels.md`](docs/hpdata-gamelevels.md)
- [`docs/game-scene-object-construction.md`](docs/game-scene-object-construction.md)
- [`docs/game-scene-resource-loading.md`](docs/game-scene-resource-loading.md)
- [`docs/playable-path.md`](docs/playable-path.md)

## EnemyActionsData WBG structural reconstruction

The clean-room `enemy_actions_wbg_document` parser now composes all **currently recovered** sections of `EnemyActionsData::loadWBGFile` in native read order:

| Stream part | Reconstructed boundary |
| --- | --- |
| Header | Fixed 16-byte prefix and primary-record count |
| A | Counted variable-length primary `ActionFrameData` records |
| B | Counted 12-byte compact records |
| C | Nested counted variable-length action-frame groups |
| D | Six or twenty groups selected by the first header word (`> 0x68` selects twenty) |
| E | Counted fixed 53-byte action-frame records |
| F | One 24-byte combo tuple per primary record |
| G | One 4-byte final-table integer per primary record; recovered reciprocal transform |

All child parsers use the bounds-checked project-owned `HPData` reader, validate cursor advancement and publish output transactionally. The composed parser reports `bytes_consumed` and `trailing_bytes` rather than inventing an EOF rejection rule. A focused synthetic smoke verifies a 156-byte A-G stream, two trailing bytes and unchanged output on truncation; the parser is compiled into the Android native target.

**Boundary:** no original WBG payload is stored in this repository. The composed format has not been verified against genuine user-owned WBG files; unresolved string/tuple meanings, `ActionFrameData` semantics, native malformed-file behavior and integration with combat remain open. Completing this parser does **not** advance P3-P9.

Evidence: [`docs/evidence/enemy-actions-wbg-document.md`](docs/evidence/enemy-actions-wbg-document.md), [`docs/evidence/unidbg-2026-10-10/enemy-actions-wbg-topology.md`](docs/evidence/unidbg-2026-10-10/enemy-actions-wbg-topology.md).

## Native dynamic evidence / unidbg

The repository contains a bounded unidbg harness and committed evidence from the 2026-10-10 native probe work.

Current evidence includes:

- DT_RELR handling needed by the tested Android runtime dependencies;
- nested dependency resolution fixes to keep the intended runtime libc++ dependency set;
- 48 loaded modules with zero unresolved symbols for the recorded harness run;
- explicit `JNI_OnLoad` completion;
- synthetic validation of the recovered `cocos2d::Decode` transform;
- isolated `CCRect::containsPoint` / `intersectsRect` behavior samples;
- `GameSaveData::Encode/Decode` dynamic comparisons and the project-owned matching codec;
- bounded `EnemyActionsData` constructor/CCString/HPData probe preparation and static stream topology evidence, alongside selected Lua/native bindings; these prerequisites do not constitute a successful end-to-end native WBG parse.

This is **not** proof that the original graphics/game runtime boots under unidbg. EGL initialization is still an explicit boundary, and native probes are kept isolated so an individual result is not overgeneralized into a runtime-compatibility claim.

See [`docs/evidence/unidbg-2026-10-10/README.md`](docs/evidence/unidbg-2026-10-10/README.md).

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
- direct-asset path resolution, JNI snapshots and pixel-store transactions;
- direct GLES2 texture-cache behavior and renderer syntax/contracts;
- recovered ExactFit/direct-sprite geometry;
- GameScene atlas-list discovery and imported plist/texture resolution;
- GameSaveData codec regression against committed native probe evidence;
- EnemyActionsData layout/dependency evidence and transactional WBG Sections A-G parsing (synthetic host smoke);
- Android Java compilation;
- NDK/build and page-size validation.

At the reviewed base revision `97e8b418494c01493035947f17c8e12efe6e427f` (2026-10-10), 21 visible GitHub Actions runs for that commit completed successfully. This is a **CI snapshot**, not a claim that device gameplay or real WBG inputs were tested. Full validation requiring a user-owned original APK remains manual-only; ordinary PR checks are designed to stay reproducible without proprietary inputs.

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
- [x] isolated native dynamic-evidence harness established
- [ ] close remaining contiguous login/runtime blockers

### M4 — First offline scene / technical-alpha path

- [x] splash/initial-scene sequencing
- [x] `HelloWorld` -> `ManagementLayer`
- [x] Management server/role models and callbacks
- [x] substantial ChooseHero presentation/state reconstruction
- [x] complete first-object record parser
- [x] retained scene instance + construction plan
- [x] live render queue
- [x] direct imported asset request/pixel staging pipeline
- [x] direct staged-pixel -> GLES2 texture synchronization
- [x] direct-file GameScene sprite renderer
- [x] recovered direct-sprite ExactFit/anchor/rotation semantics
- [x] GameScene atlas preload discovery
- [x] sprite-frame request bridge/snapshot
- [x] imported atlas plist/texture resolution
- [ ] finish server-selection end-to-end
- [ ] finish ChooseHero -> enter-game end-to-end
- [ ] parse atlas frame metadata and render `spriteFrameByName` objects
- [ ] visibly prove the complete first offline scene on device
- [ ] spawn player
- [ ] expose visible player input

### M5 — Offline gameplay

- [x] GameSaveData Encode/Decode leaf transform recovered
- [x] bounded EnemyActionsData layout/dependency evidence
- [x] recovered EnemyActions WBG Sections A-G composed into a transactional parser with synthetic host tests
- [ ] validate the composed WBG parser against genuine user-owned files and connect it to runtime consumers
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
- [`docs/game-scene-resource-loading.md`](docs/game-scene-resource-loading.md) — atlas preload and sprite-frame resource-loading evidence
- [`docs/evidence/unidbg-2026-10-10/README.md`](docs/evidence/unidbg-2026-10-10/README.md) — bounded native dynamic evidence and reproduction metadata
- [`docs/evidence/enemy-actions-wbg-document.md`](docs/evidence/enemy-actions-wbg-document.md) — recovered WBG A-G document parser, validation and unresolved semantics
- [`docs/evidence/unidbg-2026-10-10/enemy-actions-wbg-topology.md`](docs/evidence/unidbg-2026-10-10/enemy-actions-wbg-topology.md) — native read order and destination offsets
- [`docs/android-bootstrap.md`](docs/android-bootstrap.md) — Android/native bootstrap and lifecycle research
- [`docs/initial-ui-transition.md`](docs/initial-ui-transition.md) — `HelloWorld` -> `ManagementLayer`
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

Reverse-engineering findings are useful even before they become reconstructed source code. Helpful contributions include function identification, Ghidra analysis, Java/JNI mapping, Cocos2d-x matching, script/resource format research, bounded native experiments, clean-room runtime implementations, Android compatibility work and behavior comparison against the original game.

When documenting reconstructed behavior, include evidence whenever practical: binary hash/version, symbol/address, strings, xrefs, imports, call relationships or runtime observations.

Do not commit original APK/OBB files, original native game binaries, copyrighted game assets, user saves or decoded proprietary scripts. Tools, original clean-room code and reverse-engineering metadata should be sufficient for users to work from their own legally obtained copy.

## Legal notice

This is an independent preservation and reverse-engineering project and is not affiliated with or endorsed by the original developers, publishers or rights holders of Never Gone.

The repository is intended to contain **original project code, documentation and reverse-engineering metadata only**. Users are expected to provide any required original game files from their own legally obtained copy.

All trademarks and copyrighted materials belong to their respective owners.
