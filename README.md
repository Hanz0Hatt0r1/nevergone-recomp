# Never Gone Recomp

Experimental clean-room reverse-engineering and recompilation project for the Android version of **Never Gone**.

The goal is to reconstruct the original game runtime as maintainable source code for modern Android, including `arm64-v8a`, without depending on the obsolete original Android toolchain and without redistributing proprietary game binaries or assets.

> [!IMPORTANT]
> The project is **not yet a playable recompilation**. The clean runtime, imported-asset renderer, player spawn/input/action path, bounded persistence slice, substantial first-scene/SceneActions runtime and a large evidence/analysis toolchain are implemented. The dominant delivery blocker is still **one contiguous real-device P7-P10 acceptance run** with user-owned imported data, followed by the first real enemy/damage/death gameplay slice and physical arm64 validation.

## Readiness review — 2026-10-14

Assessment baseline: `main` through `b9fd09cc` (`b9fd09cc5456329248d1a59532713cb448f6a748`). This README update is documentation-only and follows that code baseline. Percentages are engineering planning estimates, not measured code coverage.

- **Strict technical-alpha gate:** **7/10 checkpoints (70%)**. P0-P6 are accepted; P7-P9 still require one contiguous retained real-device proof.
- **Engineering readiness toward technical alpha:** approximately **94%**. The scene, player/input/action and P10 persistence implementations are largely present; the remaining risk is dominated by real-device integration/evidence rather than missing core infrastructure.
- **Early playable-alpha readiness:** approximately **67%**. The technical-alpha proof is still open, and a genuine enemy interaction plus damage/death loop are not yet accepted.
- **Modern release readiness:** approximately **53%**. Runtime/tooling/resource recovery are mature, but gameplay breadth, physical arm64 evidence, release-device coverage and final hardening remain incomplete.
- **Reverse-engineering/tooling maturity:** approximately **99%** for the current workflow. Ghidra, unidbg, device capture, structural evidence, exact native cross-checks, static dispatch/vtable analysis and ML-assisted prioritization are broad and highly reproducible.

The authoritative delivery metric is [`docs/playable-path.md`](docs/playable-path.md). The milestone tracker is [#151 — M4 -> First Offline Scene / Technical Alpha path](https://github.com/Hanz0Hatt0r1/nevergone-recomp/issues/151).

### Current acceptance boundary

The next project-wide proof is one retained Android run with user-owned imported data that demonstrates, in one contiguous project-owned path:

1. **P7:** the first offline scene visibly renders;
2. **P8:** the reconstructed player visual appears at the recovered spawn point;
3. **P9:** a real Android gesture visibly changes player/control state;
4. **P10:** changed player motion survives a real process restart through the implemented project-owned sidecar.

Implementation beyond P6 does not move the strict completion marker until the preceding checkpoints are accepted in the same runtime path.

## What changed since the previous 2026-10-14 review

The previous readiness snapshot was anchored at `1385a747`. The assessed `main` is **127 commits ahead** of that point. Most of the increase is evidence/reconstruction infrastructure rather than a new playable-path checkpoint, so the strict technical-alpha gate remains unchanged.

Material additions include:

- direct SceneActions spatial-hit evaluation for the current pair, plus timed pair-only triggering;
- static direct SceneActions geometry collection and binding to the live asset store;
- composed SceneActions bound-object maps and additional recovered `klhuo`/direct geometry paths;
- stricter fail-closed SceneActions playback-slot and placement validation;
- substantially deeper indirect-call receiver recovery, including load roots, parameter-relative fields, singleton-cell provenance, PHI/default-instance recovery and mixed-receiver quarantine;
- exact `GameSceneUI`/`Controls` accessor, consumer and slot-convergence analysis;
- exact native cross-checking of Controls field stores, aliases, callback objects and register-rooted writes;
- recovery of all **78** Controls-relative call-argument offsets from exact Thumb witnesses;
- closure of retained ControlsButton/joystick aliases, scheduler callbacks and `CCCallFuncN` callback paths;
- bounded Cocos parent/touch-dispatch back-reference analysis for Controls;
- static closure of known parent-mediated Controls writes and packaged script-engine setter reachability;
- packaged `dlsym` surface closure excluding a hidden Controls script-setter route;
- exact-native reconciliation of all **14** external full-Controls-this first-hop call sites;
- broad new CI smoke coverage for the SceneActions, receiver-provenance and Controls closure work.

These additions materially reduce uncertainty around the P7-P9 runtime and the original Controls object layout/call surface, but they do **not** replace the required contiguous real-device acceptance run.

## Delivery readiness

A checkpoint counts only when every earlier checkpoint works in the same project-owned runtime path.

| ID | Checkpoint | Current state |
| --- | --- | --- |
| P0 | Clean project-owned launch | **Done** |
| P1 | User-owned original data available | **Done** |
| P2 | Reconstructed startup/login | **Done** |
| P3 | Server selection end-to-end | **Done** — issue #144 closed |
| P4 | ChooseHero end-to-end | **Done** for the tracked existing-role route — issue #145 closed |
| P5 | Deterministic enter-game transition | **Done** for the tracked existing-role route — issue #145 closed |
| P6 | GameLevels first scene-construction boundary | **Done** — issue #146 closed |
| P7 | First offline scene instantiated and visibly rendered | **In progress** — implementation and fail-closed capture tooling exist; fresh contiguous real-device proof remains |
| P8 | Player spawned | **In progress** — spawn/frame/action/render implementation exists; contiguous real-device proof remains |
| P9 | Input visibly changes player state | **In progress** — joystick/motion/facing/action implementation plus substantially stronger Controls reconstruction exist; contiguous real-device proof remains |
| P10 | Minimal save -> restart -> restore | **Implemented, proof pending** — bounded sidecar/autosave/restore runtime and host regression exist; real-device restart proof remains |

By the strict technical-alpha definition (P0-P9), the highest contiguous accepted checkpoint remains **P6: 7/10 = 70%**.

## Readiness by area

| Area | Estimated readiness | Notes |
| --- | ---: | --- |
| Build/tooling/clean runtime | **98%** | Modern Gradle/CMake app, JNI/native runtime, Lua, dual ABI and broad regression infrastructure |
| Original-data import/resource recovery | **96%** | APK/OBB import and core transformed-resource paths are established |
| Startup/login/server/ChooseHero route | **92%** | P3-P5 accepted for the tracked route; fresh-role callback continuation remains a separate boundary |
| GameLevels/scene model | **91%** | P6 accepted; retained scene/camera/render models are substantial |
| First-scene rendering | **94%** | Direct + atlas + GLES2 paths, supported plist formats, recovered lookup/geometry semantics and capture tooling exist; P7 device acceptance remains |
| Player spawn/input/action | **94%** | Spawn, motion, joystick, facing, live action playback and action-state transitions are implemented; Controls layout/callback/call-site uncertainty is substantially reduced; P8/P9 proof remains |
| SceneActions runtime | **88%** | Pair loading/playback, timed/spatial triggering, bound-object maps and direct/static geometry paths exist; some dynamic/global PRNG semantics remain bounded gaps |
| Persistence | **85%** | Minimal bounded P10 sidecar/autosave/restore is implemented; real process-restart proof remains |
| Offline combat/gameplay | **35%** | Player/action/tutorial and structural enemy/trap foundations exist, but no accepted first enemy interaction or damage/death loop |
| Modern Android runtime validation | **72%** | API 35/36 x86_64 and Android 15 16 KiB runtime coverage exist; API 35/36 arm64 physical-device proof remains |
| Static/native evidence and reproducibility | **99%** | Extensive Ghidra/unidbg/device-capture, exact Thumb/native cross-checks and fail-closed reconstruction tooling |
| ML/dispatch analysis pipeline | **92%** | Receiver provenance, structural recovery, vtable/slot inference and mixed-receiver safety gates are substantially deeper; this work improves evidence discovery but does not itself advance playable-path checkpoints |

## Immediate critical path

1. Retain one fresh contiguous real-device P7 capture from user-owned imported data showing the first offline scene actually rendered.
2. In that same path, prove the reconstructed player spawn/frame/action state and close P8.
3. Record one actual Android gesture that visibly changes player/control state and close P9; use the now-bounded Controls reconstruction only to diagnose any mismatch rather than as a substitute for device proof.
4. Perform a real process restart and retain evidence that the implemented P10 sidecar restores the expected player motion state.
5. Validate the current Android build on API 35/36 arm64 hardware and record the release-gate matrix.
6. Validate EnemyActions/TrapActions structural parsers against genuine user-owned payloads.
7. Connect the minimum genuine enemy/action data required for one first enemy interaction and damage/death loop.
8. Continue SceneActions/dispatch reconstruction only where it materially advances the first playable scene or first combat slice; unresolved behavior must remain fail-closed rather than guessed.

## Current state

`main` includes:

- a modern Gradle/CMake Android application with project-owned C++/JNI code;
- `armeabi-v7a` and `arm64-v8a` build targets;
- embedded **Lua 5.2.3**, matching the original runtime;
- deterministic decoding for transformed Lua/PNG/HPC/CSV resources;
- Android import flows for user-owned original APK and OBB data;
- reconstructed Android -> JNI -> startup/lifecycle state;
- reconstructed `AppDelegate`, splash/scene-generation and `HelloWorld` -> `ManagementLayer` behavior;
- completed tracked P3 server-selection and P4/P5 ChooseHero -> enter-game transitions;
- a bounds-checked `HPData` replacement and completed P6 first scene-construction boundary;
- retained GameLevels scene/model state, recovered camera/parallax inputs and evidence-backed construction order;
- a live GameScene render queue;
- direct-file and sprite-frame asset request domains;
- Cocos-compatible plist/atlas parsing for formats 0-3, texture resolution and atlas-frame staging;
- Java bitmap staging and transactional pixel revisions;
- GLES2 texture upload/cache and project-owned sprite drawing;
- recovered 1136x640 ExactFit geometry, anchor, scale, rotation, flip and bounded skewX behavior;
- reconstructed player spawn composition and body transform;
- recovered player motion state and fixed-clock advancement;
- recovered Android `CCJoyStick` touch geometry and pointer ownership;
- recovered joystick dock visuals and visible facing state;
- live player action document loading and frame-by-frame `MeActionsSystem` playback;
- recovered player function/action gates and selected action-state transitions;
- tutorial loading/session/gameplay state foundations;
- CharacterName asset-path/bootstrap fixes and a real-device route reaching valid `CreateCharacter` Lua dispatch;
- a reconstructed `GameSaveData::Encode/Decode` leaf codec validated against bounded native probes;
- an independent bounded P10 minimal save/autosave/restart-restore runtime;
- transactional `EnemyActionsData` structural parsing and expanded `TrapActionsData` reconstruction;
- a live SceneActions current-scene pair runtime with document/pixel staging and GLES submission;
- recovered SceneActions timed/spatial trigger contracts, event-ID mapping, bound-object maps, pair-level hit evaluation and direct/static geometry paths;
- recovered type-0 GameScene animation behavior for opacity and skewX loops;
- a repaired unidbg harness completing explicit `JNI_OnLoad` for the recorded dependency set;
- original-device derived player/enemy runtime evidence;
- a normalized P-code ML/analysis testbed with conservative labels and leakage-resistant splits;
- static and runtime indirect-dispatch analysis, receiver/vtable inference and evidence provenance controls;
- exact native Controls store/call/callback closure work covering direct stores, aliases, 78 this-derived call arguments, retained callbacks, parent/touch backrefs and external full-this first-hop sites;
- API 35/36 x86_64 lifecycle CI and Android 15 16 KiB runtime CI;
- an arm64 real-device evidence harness awaiting physical-device acceptance.

### Important current limitations

- P7 remains open because a complete accepted real-device imported-data first-scene capture has not yet been retained after the latest renderer/resource fixes.
- P8/P9 implementations are deep, but the required contiguous device proof of visible player + real touch-driven state change is still missing; the new Controls closure narrows diagnosis but does not satisfy that proof.
- P10 is implemented but has not yet been promoted because the real process restart/restore proof is still missing.
- One previously tested user-owned archive lacked a referenced `p01_i01.actData`; acceptance must use a data path that can provide the required player action asset.
- The fresh-role CharacterName route dispatches `CreateCharacter`, but its tested offline callback continuation remains separate from the already accepted existing-role P4/P5 path.
- SceneActions still has explicit evidence boundaries around some dynamic sprite/current-parent inputs and original process-global PRNG selection/order.
- Enemy/Trap action parsing remains mostly structural evidence; it is not yet an accepted combat loop.
- ML/dispatch predictions are prioritization/evidence tools, not semantic proof; uncertain or conflicting results remain gated from runtime reconstruction.
- Actual API 35/36 `arm64-v8a` physical-device evidence is still required.

## Reconstruction and analysis strategy

The project does **not** attempt a blind line-by-line rewrite of the original `libcocos2dcpp.so`.

The strategy is dependency-driven and evidence-driven:

1. recover metadata, symbols, DEX/JNI relationships and script dependencies;
2. decode user-owned resources locally with reproducible tooling;
3. execute recovered startup scripts against a clean modern runtime;
4. reconstruct state machines at verified semantic boundaries instead of copying the old ABI;
5. convert callback/save/resource payloads into bounded project-owned models before attaching them to renderer state;
6. reconstruct binary readers only through proven field widths/order and stop when evidence runs out;
7. use bounded native execution, Ghidra slices and real-device probes where static analysis is insufficient;
8. use normalized P-code/ML only to rank or classify evidence candidates, never as a substitute for required semantic proof;
9. preserve evidence provenance so natural observations, static inference and interventional probes cannot silently contaminate one another;
10. recreate only behavior required by the reachable offline path;
11. validate observable behavior on real Android hardware before declaring playable-path checkpoints complete.

## ML and indirect-dispatch testbed

The repository now includes a substantial project-local analysis pipeline under [`docs/ml/`](docs/ml/) and [`tools/ml/`](tools/ml/).

Current capabilities include:

- canonical high-P-code export with address/name leakage removed from model input;
- conservative seed/evidence labels with weighted provenance;
- deterministic train/validation/test splits grouped by normalized structure;
- structural/callgraph metrics;
- function-family mining and family-grouped validation;
- similarity indexes and similarity transfer;
- OOD/selective-confidence gates;
- hierarchical routing and production-classifier experiments;
- tiny-encoder and hybrid residual experiments;
- indirect-call pattern/slice analysis;
- static vtable prefix/catalog recovery;
- exact receiver-vtable slot resolution;
- static receiver-type/vtable inference;
- parameter-relative, singleton, PHI/default-instance and structural receiver provenance recovery;
- mixed-receiver quarantine and safety finalization;
- dispatch runtime trace ingestion, equivalence and consistency analysis;
- explicit natural/interventional evidence isolation;
- receiver-type failure auditing;
- exact native cross-checks for selected Controls call/store/callback surfaces.

This tooling is useful for prioritizing the remaining native surface and resolving indirect dispatch, but **does not increase the playable-path completion marker by itself**.

See [`docs/ml/README.md`](docs/ml/README.md).

## Established findings

The reconstruction has established, among other things, that:

- package: `com.hippiegame.nevergone`;
- original version: `1.0.9`;
- original native ABI: **`armeabi-v7a` only**;
- original native libraries: `libcocos2dcpp.so` and `libffmpeg.so`;
- the main library identifies its engine as **`cocos2d-2.1rc0-x-2.1.2`**;
- the embedded Lua runtime is **Lua 5.2.3**;
- the APK contains **107 transformed `.lua` modules** and all 107 can be decoded locally from a user-owned APK with the recovered transform;
- the static Lua graph contains 107 modules, 104 resolved edges and 101 modules reachable from `Game.StartLua`;
- the original `libcocos2dcpp.so` retains roughly **26,875 defined function symbols**;
- the launcher is `com.hippiegame.nevergone.TJ_P_01` and the Java/JNI/native lifecycle path is mapped;
- `AppDelegate::AddAllSearchPath()` and its 59 child search directories are recovered;
- `GameLevels::LoadGameLevels()` section order and the first GameScene construction boundary are reconstructed;
- the original design space is 1136x640 with Cocos ExactFit behavior;
- imported direct-file and atlas-backed pixels can be staged and rendered through project-owned GLES2 code;
- `MeActionsSystem` initial state, frame advancement, completion behavior and a substantial live playback path are reconstructed;
- horizontal controls, joystick geometry and player motion/facing state are reconstructed for the supported path;
- `GameSaveData::Encode/Decode` byte transform is recovered, while P10 intentionally uses a bounded independent project-owned sidecar;
- the SceneActions timed trigger schedule, spatial dispatch contract, current-pair binding, event-ID source and several direct/static geometry paths are recovered for the supported path;
- the Controls object now has substantially stronger exact-native evidence for field writes, aliases, callbacks, parent/touch backrefs and external full-this first-hop calls;
- API 35/36 x86_64 lifecycle tests and an Android 15 16 KiB runtime test pass in CI.

## Repository layout

```text
nevergone-recomp/
├── README.md
├── android/              # modern Gradle/NDK app and clean-room runtime
│   ├── app/
│   └── README.md
├── docs/                 # reconstruction notes and evidence
│   ├── evidence/         # bounded native/static/device evidence
│   └── ml/               # ML/static-dispatch analysis documentation
├── tools/                # decoders, Ghidra/unidbg/device/ML tooling
├── third_party/_local/   # ignored locally fetched dependencies
└── .github/              # CI workflows
```

Original Never Gone APK/OBB files, decoded proprietary scripts, saves, raw process memory and copyrighted assets are intentionally not stored in the repository.

## Reconstructed runtime flow

```text
Android launcher/lifecycle
        |
        v
reconstructed AppDelegate
        |
        v
HelloWorld / ManagementLayer
        |
        +--> server selection (P3)
        +--> role selection / ChooseHero (P4)
        `--> enter-game (P5)
                 |
                 v
          GameLevels model (P6)
                 |
                 v
       retained scene + camera
                 |
                 v
       construction/render queue
          /       |        \
         v        v         v
   scene assets  player   SceneActions
         \        |         /
          \       v        /
             GLES2 renderer
                  |
                  v
          joystick / controls
                  |
                  v
        player motion/actions
                  |
                  v
       minimal P10 autosave
                  |
                  v
        process restart/restore
```

The runtime does **not** link against the original `libcocos2dcpp.so`.

## First-scene and player reconstruction

The first-scene stack separates binary evidence, project-owned state and rendering:

- GameLevels/HPData parsing is bounds-checked and transactional;
- retained scene state feeds ordered camera/construction/render models;
- direct-file resources and `spriteFrameByName()` resources use recovered search/cache semantics;
- plist formats 0-3 and rotated atlas frames normalize into the bounded pixel/GLES staging path;
- imported pixels are published transactionally and uploaded into GLES2 textures;
- player spawn/motion state is project-owned and generation-bound;
- live action playback consumes loaded action documents and the recovered frame cursor;
- Android motion events feed recovered joystick geometry and movement/facing state;
- tutorial/action-button foundations can drive supported player action-state changes;
- Controls static/native closure work now bounds many original object-layout, callback and first-hop call paths used to diagnose input mismatches.

**Remaining acceptance boundary:** retain one real-device run proving scene + player + real input in the same imported-data path.

## SceneActions reconstruction

The current SceneActions path includes:

- parsed pair documents and imported frame-resource resolution;
- initial/current frame pixel staging and GLES submission;
- recovered born/idle playback transitions;
- one current-scene pair binding with fail-closed preflight;
- recovered timed trigger schedule and pair-only timed evaluation;
- recovered spatial trigger predicate/order and direct pair hit evaluation;
- `GameSceneObject` event-ID recovery from parsed object data;
- composition of timed + spatial gating into the live runtime start path;
- typed spatial-object construction from parsed scene records;
- bound-object maps linking reconstructed scene records to SceneActions geometry;
- static direct geometry extraction and asset-store binding;
- additional sprite/fallback bounds, `klhuo` bounds and parent-position reconstruction helpers;
- fail-closed slot and nonfinite-placement validation.

**Remaining boundary:** complete only the dynamic geometry/current-parent inputs and original PRNG semantics actually required by the first playable scene.

## Persistence slice

P10 uses a deliberately small project-owned sidecar under app-private storage rather than claiming compatibility with unproven original save containers.

The bounded record stores the minimum visible restart state: scene identity, player identity and player motion offset. It validates magic/version/size/checksum/ranges, writes transactionally through a temporary file, autosaves changed motion at a bounded cadence, and applies a matching record only after normal runtime flow has independently reconstructed the corresponding scene/player spawn.

See [`docs/minimal-save-restart-restore.md`](docs/minimal-save-restart-restore.md).

## Enemy/Trap action reconstruction

EnemyActions and TrapActions currently provide structural, bounds-checked project-owned models and evidence needed for later combat integration. These parsers do **not** by themselves constitute combat.

The next gameplay milestone is intentionally small: validate genuine user-owned action data, instantiate one enemy interaction, then implement the minimum damage/death loop required for early playable alpha.

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

Synthetic fixtures and host probes are used wherever possible so proprietary resources do not need to live in source control.

Coverage includes:

- Lua/startup compatibility and lifecycle;
- server/role/ChooseHero transitions;
- HPData/GameLevels parsing and failure bounds;
- retained scene/camera state and construction order;
- render-queue ordering and resource domains;
- direct-file and all supported plist/atlas asset staging;
- GLES2 texture/cache/render contracts and sprite geometry;
- player spawn/motion/joystick/facing state;
- player action loading, timing, frame advancement, function/action gates and live playback;
- tutorial action/gameplay state foundations;
- P10 minimal save/autosave/restart-restore regression;
- EnemyActions/TrapActions structural parsing;
- SceneActions document/frame/render/playback/timed/spatial trigger, bound-object and direct-geometry contracts;
- Ghidra/unidbg evidence tooling;
- ML corpus/classification/OOD/family/similarity smoke tests;
- indirect-dispatch/vtable/receiver-provenance analysis smoke tests;
- exact Controls store/alias/callback/call-argument/parent/dlsym/full-this closure smoke tests;
- API 35/36 lifecycle runtime CI;
- Android 15 16 KiB runtime CI;
- arm64 real-device evidence tooling.

Device gameplay acceptance still requires user-owned original data and is intentionally not represented by committed proprietary fixtures.

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
- [x] retained scene/camera/construction/render pipeline
- [x] Cocos-compatible direct-file and atlas asset pipelines
- [x] player spawn/motion/action runtime foundations
- [x] recovered Android joystick/facing/dock state
- [x] substantial exact-native Controls layout/callback/call-surface closure
- [x] P10 bounded minimal persistence implementation
- [ ] P7 retained real-device first-scene proof
- [ ] P8 retained real-device player proof
- [ ] P9 retained real-device visible-input proof
- [ ] P10 retained real process-restart/restore proof

### M5 — Offline gameplay

- [x] GameSaveData Encode/Decode leaf transform recovered
- [x] live player action playback for the supported path
- [x] tutorial/action-state foundations
- [x] EnemyActions structural reconstruction
- [x] substantial TrapActions structural reconstruction
- [x] substantial bounded SceneActions runtime/trigger/geometry reconstruction
- [ ] validate action parsers against genuine user-owned files
- [ ] first enemy interaction
- [ ] damage/death loop
- [ ] minimum combat systems
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
- [`docs/ml/README.md`](docs/ml/README.md) — normalized P-code/ML and static-dispatch testbed
- [`docs/ml/PRODUCTION_CLASSIFIER.md`](docs/ml/PRODUCTION_CLASSIFIER.md) — current classifier/routing contract
- [`docs/ml/OOD_GATE.md`](docs/ml/OOD_GATE.md) — selective/OOD gating
- [`docs/ml/receiver-vtable-slot-resolution.md`](docs/ml/receiver-vtable-slot-resolution.md) — exact receiver/vtable slot analysis
- [`docs/minimal-save-restart-restore.md`](docs/minimal-save-restart-restore.md) — bounded P10 persistence slice
- [`docs/live-player-action-playback.md`](docs/live-player-action-playback.md) — recovered live player action playback path
- [`docs/player-input-runtime-evidence.md`](docs/player-input-runtime-evidence.md) — player input/runtime evidence
- [`docs/tutorial-gameplay-state.md`](docs/tutorial-gameplay-state.md) — tutorial gameplay-state reconstruction
- [`docs/scene-action-pair-runtime.md`](docs/scene-action-pair-runtime.md) — SceneActions pair runtime
- [`docs/scene-actions-device-probe.md`](docs/scene-actions-device-probe.md) — bounded SceneActions device probe
- [`docs/hpdata-gamelevels.md`](docs/hpdata-gamelevels.md) — HPRange and scene parsing evidence
- [`docs/game-scene-object-construction.md`](docs/game-scene-object-construction.md) — scene traversal/construction evidence
- [`docs/game-scene-resource-loading.md`](docs/game-scene-resource-loading.md) — atlas/sprite-frame resource-loading evidence
- [`docs/evidence/unidbg-2026-10-10/README.md`](docs/evidence/unidbg-2026-10-10/README.md) — bounded native dynamic evidence
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
