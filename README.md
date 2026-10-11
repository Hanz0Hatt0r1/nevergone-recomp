# Never Gone Recomp

Experimental clean-room reverse-engineering and recompilation project for the Android version of **Never Gone**.

The goal is to reconstruct the original game runtime as maintainable source code for modern Android, including `arm64-v8a`, without depending on the obsolete original Android toolchain and without redistributing proprietary game binaries or assets.

> [!IMPORTANT]
> The project is **not yet a playable recompilation**. It is now beyond the bootstrap/research phase: the clean runtime reaches the first-scene construction boundary, the renderer and imported-asset pipeline are in place, the first player sprite/control path is reconstructed, and modern Android lifecycle/page-size CI exists. The main blocker is no longer missing core infrastructure; it is obtaining **contiguous real-device evidence** for P7-P9 and then adding the first gameplay/persistence slice.

## Readiness review — 2026-10-11

Assessment is based on current `main` through commit `588530c` and the project milestone/issues. Percentages are engineering planning estimates, not measured code coverage.

- **Strict technical-alpha gate:** P0-P6 are complete, while P7-P9 still require contiguous real-device proof: **7/10 checkpoints (70%)**.
- **Engineering readiness toward technical alpha:** approximately **85%**. Most of the missing P7-P9 implementation exists; validation/integration is now the dominant risk.
- **Early playable alpha:** **not reached**. A real-device first-scene/player/input proof, first enemy interaction, damage/death loop and minimal persistence are still required.
- **Release readiness:** approximately **40%**. Core runtime/tooling is strong, but gameplay breadth, persistence, arm64 device validation and release hardening remain incomplete.

The authoritative milestone tracker is [#151 — M4 -> First Offline Scene / Technical Alpha path](https://github.com/Hanz0Hatt0r1/nevergone-recomp/issues/151).

### Current acceptance boundary

The next project-wide proof is a real Android run using user-owned imported data that demonstrates, in one contiguous project-owned path:

1. P7: first offline scene visibly renders from imported data;
2. P8: the reconstructed player visual is present at the recovered spawn point;
3. P9: an actual Android gesture changes visible player/control state;
4. P10 follows with minimal save -> process restart -> restore.

Recent work already supplies a fail-closed P7 device-capture harness and the P8/P9 runtime pieces. The remaining issue is evidence and final route integration, not a missing rendering/input architecture.

## Current state

`main` now includes:

- a modern Gradle/CMake Android application with project-owned C++/JNI code;
- `armeabi-v7a` and `arm64-v8a` build targets;
- embedded **Lua 5.2.3**, matching the original runtime;
- deterministic decoding for transformed Lua/PNG/HPC/CSV resources;
- Android import flows for user-owned original APK and OBB data;
- reconstructed Android -> JNI -> startup/lifecycle state;
- reconstructed `AppDelegate`, splash/scene-generation and `HelloWorld` -> `ManagementLayer` behavior;
- completed tracked P3 server-selection and P4/P5 ChooseHero -> enter-game transitions;
- a bounds-checked `HPData` replacement and the completed tracked P6 first scene-construction boundary;
- retained GameLevels scene/model state and evidence-backed construction order;
- a live GameScene render queue;
- direct-file and sprite-frame asset request domains;
- imported plist/atlas parsing, texture resolution and atlas-frame staging;
- Java `BitmapFactory` staging and transactional ARGB pixel revisions;
- GLES2 texture upload/cache and project-owned sprite drawing;
- recovered 1136x640 ExactFit geometry, anchor, scale, rotation and flip behavior;
- a project-owned initial player sprite path from `MeActionsSystem` frame state into the atlas/GLES renderer;
- role-scoped player atlas resolution separated from ordinary scene atlas resolution;
- recovered player spawn composition and body transform;
- an evidence-backed player control state (`0x12` / `0x13`) connected to visible `flipX` state;
- recovered Android `CCJoyStick` touch geometry and pointer ownership;
- recovered joystick dock visuals (`Dock.png` idle, `Dock_s.png` while active) in the live render queue;
- CharacterName asset-path/bootstrap fixes and a real-device path that reaches a valid `CreateCharacter` Lua dispatch;
- a reconstructed `GameSaveData::Encode/Decode` leaf codec validated against bounded native probes;
- a transactional `EnemyActionsData` WBG Sections A-G structural parser with synthetic host coverage;
- a repaired unidbg harness that completes explicit `JNI_OnLoad` for the recorded dependency set and runs bounded synthetic probes;
- original-device derived memory/frame evidence for `MeActionsSystem` and `EnemyActionsSystem` fields;
- API 35 and API 36 x86_64 lifecycle CI;
- Android 15 16 KiB runtime CI with `PAGE_SIZE=16384`;
- an arm64 real-device evidence harness awaiting execution on an API 35/36 arm64 target.

### Important current limitations

- P7 is still open because the complete first scene has not yet been accepted from a retained real-device imported-data capture.
- P8/P9 code is substantially implemented, but the project still lacks the real-device proof that an Android gesture visibly changes the imported player/control rendering.
- One tested user-owned archive did not contain the referenced `p01_i01.actData`, so the corresponding player action cannot be visually proven from that data set.
- The fresh-role CharacterName route dispatches `CreateCharacter`, but the tested offline path did not receive a create-character callback to continue into the scene. This is a separate route-integration boundary, not a reason to reopen the already-completed tracked existing-role P4/P5 path.
- EnemyActions parsing remains **structural evidence**, not functioning combat.
- Actual API 35/36 `arm64-v8a` runtime evidence is still required.

## Delivery readiness

The primary delivery metric is [`docs/playable-path.md`](docs/playable-path.md). A checkpoint counts only when every earlier checkpoint works in the same project-owned runtime path.

| ID | Checkpoint | Current state |
| --- | --- | --- |
| P0 | Clean project-owned launch | **Done** |
| P1 | User-owned original data available | **Done** |
| P2 | Reconstructed startup/login | **Done** |
| P3 | Server selection end-to-end | **Done** — tracked issue #144 closed |
| P4 | ChooseHero end-to-end | **Done** — tracked issue #145 closed |
| P5 | Deterministic enter-game transition | **Done** — tracked issue #145 closed |
| P6 | GameLevels data sufficient for first scene-construction boundary | **Done** — tracked issue #146 closed |
| P7 | First offline scene instantiated and visibly rendered | **In progress** — implementation/capture harness exists; real-device proof remains |
| P8 | Player spawned | **In progress** — spawn/frame/render path implemented; device proof remains |
| P9 | Input visibly changes player state | **In progress** — recovered joystick/facing/dock path implemented; device proof remains |
| P10 | Minimal save -> restart -> restore | **Not yet** — leaf save codec recovered |

By the strict technical-alpha definition (P0-P9), the contiguous completion marker is **P6: 7/10 = 70%**.

This strict number understates implementation depth in P7-P9: much of the downstream code exists, but the project intentionally does not promote those checkpoints until the required real-device imported-data evidence is retained and validated.

## Readiness by area

| Area | Estimated readiness | Notes |
| --- | ---: | --- |
| Build/tooling/clean runtime | **95%** | Modern app, JNI/native runtime, Lua, dual ABI, broad CI |
| Original-data import/resource recovery | **95%** | APK/OBB import and core transforms are established |
| Startup/login/server/ChooseHero route | **90%** | P3-P5 tracked gates closed; fresh-role callback path still has an offline boundary |
| GameLevels/scene model | **85%** | P6 gate closed; enough model exists for first-scene construction work |
| First-scene rendering | **85%** | Direct + atlas pipelines exist; retained real-device P7 proof is missing |
| Player spawn/input | **80%** | Initial sprite, spawn composition, joystick, facing and dock states are implemented; P8/P9 device proof missing |
| Offline combat/gameplay | **20%** | Structural action data exists; enemy instantiation and combat loop do not |
| Persistence | **30%** | Leaf codec recovered; P10 vertical slice not implemented |
| Modern Android runtime validation | **70%** | API 35/36 x86_64 + 16 KiB runtime CI pass; API 35/36 arm64 physical-device proof pending |
| Evidence/reproducibility | **95%** | Extensive bounded docs, CI, Ghidra/unidbg/device capture tooling |

## Immediate critical path

1. Run and retain the P7 real-device evidence flow with a user-owned data set that can actually reach/render the first offline scene.
2. Close the remaining imported-data blocker for the initial player action resource (`p01_i01.actData` or an equivalent valid role/data path) without committing proprietary assets.
3. Use the merged player sprite + joystick/dock path to record one Android gesture producing a visible state change; close P8/P9.
4. Resolve the fresh-role `CreateCharacter` offline callback boundary if fresh-role creation is required for the chosen contiguous acceptance path.
5. Implement P10 minimal project-owned save -> restart -> restore.
6. Validate the WBG parser against genuine user-owned inputs and connect only the minimum enemy/action behavior needed for the first combat slice.
7. Add one enemy interaction and damage/death loop.
8. Run the arm64 evidence harness on an API 35/36 arm64 device and record the release-gate matrix.

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
- the launcher is `com.hippiegame.nevergone.TJ_P_01` and the Java/JNI/native lifecycle path is mapped;
- `AppDelegate::AddAllSearchPath()` and its 59 child search directories have been recovered;
- startup reaches `Game.StartLua`, `Game.ClientRequire`, `ShareLogic.require`, `HelloWorld` and `ManagementLayer` through known paths;
- original `NewServerList` preselection/tap-vs-drag/one-shot confirmation behavior is reconstructed;
- the shipped ChooseHero `PartThree()` / `BalckCloud()` path and thunder scheduling behavior are substantially reconstructed;
- the original 32-bit `HPRange` is `{byte_offset, byte_length}`;
- `GameLevels::LoadGameLevels()` loads scene, action, global and port-node sections in a recovered fixed order;
- the first GameScene construction boundary needed by tracked P6 is implemented with bounds-checked project-owned models;
- the visual scene path preserves recovered ordered layer traversal;
- type-0 visual objects use evidence-backed resource selection and transforms where proven;
- the original design space is 1136x640 with Cocos ExactFit behavior;
- imported direct-file and atlas-backed pixels can be staged and uploaded into the project-owned GLES2 renderer;
- player atlas requests are isolated from scene atlas requests;
- `MeActionsSystem::initWithFile()` / `play()` establish initial player frame state `current=0`, `previous=-1` before the first primary frame lookup;
- the recovered player visual position composes the bootstrap point with the type-6 scene offset;
- the recovered horizontal Controls states map to player sprite `flipX` changes;
- the recovered fixed joystick node is at design coordinate `(204,173)`, with begin hit radius `165` and update radius `290`;
- joystick dock visuals switch between `Dock.png` and `Dock_s.png` for inactive/active touch ownership;
- `GameSaveData::Encode/Decode` use the recovered byte transform with a counter cycling `0..126`;
- bounded unidbg work completed explicit `JNI_OnLoad` after loading the recorded runtime dependency set and validated selected isolated helpers/transforms;
- `EnemyActionsData::loadWBGFile` is confirmed as an HPData-backed dependency and the recovered A-G stream topology is parsed transactionally in project-owned code;
- original-device memory evidence corroborates changing runtime frame-counter fields while retaining the static/instruction-path evidence as the semantic source of truth;
- API 35/36 x86_64 lifecycle tests and an Android 15 16 KiB runtime test pass in CI.

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
| Server selection P3 | Done |
| ChooseHero / enter-game P4-P5 | Done for tracked route |
| Fresh-role CharacterName -> callback continuation | In progress |
| `HPData` foundation | Done for current parser needs |
| GameLevels P6 first-scene construction boundary | Done |
| Retained scene instance / construction plan | Implemented |
| Live GameScene render queue | Implemented |
| Direct imported sprite pipeline | Implemented |
| Atlas plist/frame/texture pipeline | Implemented for current scene/player needs |
| Direct + atlas GLES2 sprite drawing | Implemented |
| P7 complete first offline scene evidence | Pending real-device acceptance |
| Initial player frame/spawn/render path | Implemented |
| Recovered joystick input/facing path | Implemented |
| Recovered joystick dock visual path | Implemented |
| P8/P9 real-device visible proof | Pending |
| `GameSaveData` Encode/Decode leaf codec | Implemented |
| P10 minimal save/restart/restore | Not yet |
| EnemyActions WBG Sections A-G parser | Implemented structurally; real-file/gameplay proof pending |
| Offline combat/gameplay | Not yet functional |
| `arm64-v8a` build target | Done |
| API 35/36 x86_64 lifecycle runtime CI | Passing |
| Android 15 16 KiB runtime CI | Passing |
| API 35/36 arm64 physical-device validation | Pending |
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
7. use bounded native execution (including unidbg probes) where static analysis is insufficient;
8. preserve recovered traversal, transforms and resource-selection semantics separately from unknown gameplay meaning;
9. build project-owned scene/runtime state from those proven models;
10. recreate only the behavior required by the reachable offline path;
11. validate observable behavior on real Android hardware before declaring a playable-path checkpoint complete.

## Repository layout

```text
nevergone-recomp/
├── README.md
├── android/              # modern Gradle/NDK app and clean-room runtime
│   ├── app/
│   └── README.md
├── docs/                 # reverse-engineering evidence and reconstruction notes
│   └── evidence/         # bounded native/static/device evidence
├── tools/                # decoders, Ghidra/unidbg/device probes and validation tools
├── third_party/_local/   # ignored locally fetched dependencies
└── .github/              # CI workflows
```

Original Never Gone APK/OBB files, decoded proprietary scripts, saves, raw process memory and copyrighted assets are intentionally not stored in the repository.

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
        +--> server selection (P3)
        +--> role selection / ChooseHero (P4)
        `--> enter-game (P5)
                 |
                 v
          GameLevels model (P6)
                 |
                 v
        retained scene instance
                 |
                 v
       construction/render queue
           /             \
          v               v
 direct-file requests   sprite-frame requests
          |               |
          v               v
  ARGB pixel staging   scene/player atlas domains
          \               /
           v             v
             GLES2 textures
                  |
                  v
           scene/player sprites
                  |
                  v
      recovered joystick/control state
```

The runtime does **not** link against the original `libcocos2dcpp.so`.

## First-scene and player reconstruction

The first-scene stack now separates binary evidence, project-owned state and rendering:

- GameLevels/HPData parsing is bounds-checked and transactional;
- the tracked P6 scene-construction boundary is complete;
- retained scene state feeds an ordered construction/render plan;
- direct-file resources and `spriteFrameByName()` resources use separate explicit request paths;
- scene atlases and selected-player atlases are resolved in separate domains;
- plist frame metadata is consumed by the project-owned atlas staging path;
- imported pixels are published transactionally and uploaded into GLES2 textures;
- the first reconstructed player frame uses the recovered `MeActionsSystem` initial state and body transform;
- the player spawn point uses the recovered bootstrap + type-6 scene-offset composition;
- player-facing state is driven by the recovered horizontal control states;
- Android motion events feed the recovered `CCJoyStick` geometry;
- joystick dock texture state is rendered as a screen-space overlay.

**Remaining boundary:** retain an accepted real-device capture that proves the scene, player and touch-driven visual state from user-owned imported data in the same contiguous runtime path.

## EnemyActionsData WBG structural reconstruction

The clean-room `enemy_actions_wbg_document` parser composes all currently recovered sections of `EnemyActionsData::loadWBGFile` in native read order:

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

All child parsers use the bounds-checked project-owned `HPData` reader, validate cursor advancement and publish output transactionally.

**Boundary:** the format still needs validation against genuine user-owned WBG payloads, semantic interpretation and runtime enemy/action consumers. This work does not by itself constitute combat.

See [`docs/evidence/enemy-actions-wbg-document.md`](docs/evidence/enemy-actions-wbg-document.md).

## Native dynamic and device evidence

The repository contains bounded unidbg, Ghidra and real-device tooling. Current evidence includes:

- repaired native dependency loading and explicit `JNI_OnLoad` completion for the recorded unidbg run;
- synthetic validation of the recovered asset decoder and selected Cocos geometry helpers;
- `GameSaveData::Encode/Decode` comparisons against native probes;
- bounded EnemyActions layout/topology evidence;
- derived original-device vtable/frame-field observations for `MeActionsSystem` and `EnemyActionsSystem`;
- a full-readable-memory capture tool for local/private evidence collection;
- a fail-closed first-scene Android capture/validator;
- arm64 retained-evidence tooling for API 35/36 hardware validation.

Raw proprietary process memory, screenshots containing game assets and original binaries remain local and are not committed.

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

A full Android debug build was revalidated after the recent player-bootstrap/CharacterName fixes for both configured ARM ABIs.

## Testing and CI

Synthetic fixtures and host probes are used wherever possible so proprietary resources do not need to live in source control.

Coverage includes:

- Lua/startup compatibility;
- AppDelegate/lifecycle and initial UI state;
- server/role/ChooseHero transitions;
- HPData/GameLevels parsing and failure bounds;
- retained scene state and construction order;
- render-queue ordering and resource domains;
- direct-file and atlas asset staging;
- GLES2 texture/cache/render contracts;
- ExactFit/sprite geometry;
- player initial-frame/spawn/body transform;
- player atlas isolation;
- recovered joystick input, facing and dock visual states;
- GameSaveData codec regression;
- EnemyActions WBG structural parsing;
- Ghidra/unidbg evidence tooling;
- API 35/36 lifecycle runtime CI;
- Android 15 16 KiB runtime CI;
- arm64 real-device evidence validation tooling.

Device gameplay evidence still requires user-owned original data and is intentionally not represented by proprietary fixtures in CI.

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
- [x] tracked server/role/enter-game path reaches P6 construction boundary

### M4 — First offline scene / technical-alpha path

- [x] P3 server selection
- [x] P4/P5 ChooseHero -> enter-game tracked route
- [x] P6 GameLevels first scene-construction boundary
- [x] retained scene instance + construction/render pipeline
- [x] direct-file and atlas asset pipelines
- [x] initial player sprite/frame/spawn path
- [x] recovered Android joystick input state
- [x] recovered visible player-facing state
- [x] recovered joystick dock visual state
- [ ] P7 retained real-device first-scene proof
- [ ] P8 retained real-device player proof
- [ ] P9 retained real-device visible input proof
- [ ] P10 minimal save/restart/restore

### M5 — Offline gameplay

- [x] GameSaveData Encode/Decode leaf transform recovered
- [x] bounded EnemyActionsData layout/dependency evidence
- [x] recovered EnemyActions WBG Sections A-G structural parser
- [ ] validate WBG parser against genuine user-owned files
- [ ] first enemy interaction
- [ ] damage/death loop
- [ ] minimum combat systems
- [ ] minimal persistence vertical slice
- [ ] broader inventory/equipment/audio/video/progression work

### M6 — Modern Android release target

- [x] `arm64-v8a` build target configured
- [x] 16 KiB build/page-alignment validation
- [x] API 35 x86_64 lifecycle runtime CI
- [x] API 36 x86_64 lifecycle runtime CI
- [x] Android 15 16 KiB runtime CI
- [ ] API 35/36 arm64 physical-device validation
- [ ] release-gate device matrix
- [ ] reproducible release build and final hardening

## Documentation

Key documents:

- [`docs/playable-path.md`](docs/playable-path.md) — primary delivery metric and P0-P10 definitions
- [`docs/roadmap.md`](docs/roadmap.md) — phased reconstruction plan
- [`docs/hpdata-gamelevels.md`](docs/hpdata-gamelevels.md) — HPRange and binary scene parsing evidence
- [`docs/game-scene-object-construction.md`](docs/game-scene-object-construction.md) — scene traversal/construction evidence
- [`docs/game-scene-resource-loading.md`](docs/game-scene-resource-loading.md) — atlas/sprite-frame resource-loading evidence
- [`docs/evidence/unidbg-2026-10-10/README.md`](docs/evidence/unidbg-2026-10-10/README.md) — bounded native dynamic evidence
- [`docs/evidence/enemy-actions-wbg-document.md`](docs/evidence/enemy-actions-wbg-document.md) — WBG A-G structural parser evidence
- [`docs/evidence/original-runtime-memory-2026-10-11.md`](docs/evidence/original-runtime-memory-2026-10-11.md) — derived original-device memory/frame observations
- [`docs/server-selection-runtime.md`](docs/server-selection-runtime.md) — `NewServerList` behavior
- [`docs/jni-map.md`](docs/jni-map.md) — Java/JNI/native mapping
- [`docs/native-analysis.md`](docs/native-analysis.md) — native symbol/runtime evidence
- [`docs/resource-decoding.md`](docs/resource-decoding.md) — recovered resource transform
- [`docs/lua-dependency-map.md`](docs/lua-dependency-map.md) — script dependency graph
- [`docs/lua-native-api-map.md`](docs/lua-native-api-map.md) — Lua/native compatibility surface

## Contributing

Useful contributions include function identification, Ghidra analysis, Java/JNI mapping, Cocos2d-x matching, script/resource-format research, bounded native experiments, clean-room runtime implementations, Android compatibility work and observable behavior comparison against the original game.

When documenting reconstructed behavior, include evidence whenever practical: binary hash/version, symbol/address, strings, xrefs, imports, call relationships or bounded runtime observations.

Do not commit original APK/OBB files, original native game binaries, copyrighted assets, user saves, decoded proprietary scripts, raw process memory or other private device data.

## Legal notice

This is an independent preservation and reverse-engineering project and is not affiliated with or endorsed by the original developers, publishers or rights holders of Never Gone.

The repository is intended to contain **original project code, documentation and reverse-engineering metadata only**. Users are expected to provide any required original game files from their own legally obtained copy.

All trademarks and copyrighted materials belong to their respective owners.
