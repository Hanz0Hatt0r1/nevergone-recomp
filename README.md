# Never Gone Recomp

Experimental clean-room reverse-engineering and recompilation project for the Android version of **Never Gone**.

The goal is to reconstruct the original game runtime as maintainable source code for modern Android, including `arm64-v8a`, without depending on the obsolete original Android toolchain and without redistributing proprietary game binaries or assets.

> [!IMPORTANT]
> The project is **not yet a playable recompilation**. The clean runtime, imported-resource pipeline, first-scene renderer, player spawn/input/action path, fresh-role tutorial bootstrap, bounded persistence slice and a substantial first-combat reconstruction are implemented. The hard acceptance boundary is still one contiguous real-device P7-P10 proof with user-owned imported data. After that, the main gameplay blocker is the live enemy actor/render/combat bridge for the first tutorial Attack wave.

## Readiness review — 2026-10-17

Assessment baseline: `main` through `e988acb1` (`e988acb1d44d07ceac8338d0fbd177f7471e4587`). This README update is documentation-only and follows that code baseline. Percentages below are engineering planning estimates, not measured code coverage.

- **Strict technical-alpha gate:** **7/10 checkpoints (70%)**. P0-P6 are accepted; P7-P9 still require one contiguous retained real-device proof.
- **Engineering readiness toward technical alpha:** approximately **96%**. The remaining technical-alpha risk is dominated by real-device integration/acceptance rather than missing scene/player/input infrastructure.
- **First tutorial playable-slice engineering readiness:** approximately **82%**. Fresh-role `cp_lesson`, LoadingLayer/LessonOverlay progression, player control, tutorial state, Attack-wave planning, enemy identity/base-HP mapping and health/death semantics are reconstructed; visible enemy actors and real combat callbacks are still missing.
- **Early playable-alpha readiness:** approximately **74%**. Technical-alpha proof is still open, and the enemy actor/damage/death loop has not yet been accepted end-to-end.
- **Modern release readiness:** approximately **57%**. Runtime/tooling/resource recovery are mature, while physical arm64 validation, broader gameplay, device coverage and release hardening remain incomplete.
- **Reverse-engineering/tooling maturity:** approximately **99%** for the current workflow. Ghidra, unidbg, device capture, exact native cross-checks, static dispatch/vtable analysis, ML-assisted prioritization and the `revenginstr` extraction/parity work are broad and reproducible.

The authoritative delivery metric is [`docs/playable-path.md`](docs/playable-path.md). The milestone tracker is [#151 — M4 -> First Offline Scene / Technical Alpha path](https://github.com/Hanz0Hatt0r1/nevergone-recomp/issues/151).

### Current acceptance boundary

The next project-wide proof is one retained Android run with user-owned imported data that demonstrates, in one contiguous project-owned path:

1. **P7:** the first offline scene visibly renders;
2. **P8:** the reconstructed player visual appears at the recovered spawn point;
3. **P9:** a real Android gesture visibly changes player/control state;
4. **P10:** changed player motion survives a real process restart through the implemented project-owned sidecar.

Implementation beyond P6 does not move the strict completion marker until the preceding checkpoints are accepted in the same runtime path.

## What changed since the 2026-10-14 review

The previous readiness baseline was `b9fd09cc`. The assessed `main` is **291 commits ahead** of that point. The strict P0-P9 acceptance marker has not moved, but the implementation behind P7-P9 and the first tutorial/combat slice has advanced materially.

Key changes:

- **OBB import now scans the actual archive structure before extracting resources.** The importer no longer assumes an archive-root `assets/` directory; it detects the logical game-resource root from recovered Never Gone paths and then imports the sibling tree. This supersedes earlier conclusions that a required asset was necessarily absent merely because it was not found under the old imported layout.
- **P7 player atlas resolution now follows the recovered SpriteFrameCache preload family.** Existing `pNN_CC_01..09` default atlases are loaded in numeric order, with the exact action atlas added only when present. Device diagnostics also expose the exact failed player-frame request.
- **Fresh-role creation now has a reconstructed tutorial handoff.** Successful `CreateCharacter` flows into `cp_lesson.glData`, the recovered LoadingLayer switch timing, the exact 0.5 s scene-switch boundary, the recovered 0.2 s hide fade and the initial LessonOverlay route.
- **Tutorial UI/state reconstruction is substantially deeper.** LessonOverlay, dialogue/interlude state, lesson progression, loading/fade behavior and related asset loaders are now project-owned runtime paths rather than only notes/foundations.
- **The first tutorial Attack wave now has a live evidence-backed gate.** `PLAYERFACEDIR`, scene limits and enemy identity/base-health fields are derived from current reconstructed runtime/imported scene data instead of hard-coded test constants.
- **Enemy wave identity and health/death semantics are reconstructed.** The runtime has generation-safe roster/actor-binding identities, fixed-tick spawn planning, current/max-health state and the recovered `EnemyObject::isDead()` contract needed by the first combat lesson.
- **The remaining combat boundary is narrower.** The major missing link is no longer basic tutorial orchestration; it is materializing visible/interactive enemy actors from imported EnemyActions data and feeding real hit/damage/death state back through the retained wave identity.
- **Generic analysis machinery is being extracted to `revenginstr`.** Shared P-code, vtable, indirect-call, receiver, evidence, selective-classifier, accept-router and frozen-validation mechanisms now have Nevergone parity/adapters while game-specific labels, catalogs and semantic evidence remain in this repository.
- **ML production work is now evaluation-driven rather than blindly iterating.** v5.4 remains the frozen classifier champion unless later constrained GAME calibration beats its family-held-out metrics under the same leakage-safe protocol.

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
| P7 | First offline scene instantiated and visibly rendered | **In progress** — renderer/import/capture implementation is deep; fresh contiguous real-device proof remains |
| P8 | Player spawned | **In progress** — spawn/frame/action/render and preload-cache paths exist; contiguous real-device proof remains |
| P9 | Input visibly changes player state | **In progress** — joystick/motion/facing/action paths exist; contiguous real-device proof remains |
| P10 | Minimal save -> restart -> restore | **Implemented, proof pending** — bounded sidecar/autosave/restore runtime and host regression exist; real-device restart proof remains |

By the strict technical-alpha definition (P0-P9), the highest contiguous accepted checkpoint remains **P6: 7/10 = 70%**.

## Readiness by area

| Area | Estimated readiness | Notes |
| --- | ---: | --- |
| Build/tooling/clean runtime | **99%** | Modern Gradle/CMake app, JNI/native runtime, Lua, dual ABI and broad regression infrastructure |
| Original-data import/resource recovery | **98%** | APK/OBB import, transformed-resource decoding and two-pass OBB root detection are implemented; latest OBB fix still needs the acceptance-device rerun |
| Startup/login/server/ChooseHero route | **96%** | Existing-role P3-P5 are accepted; fresh-role creation now enters the reconstructed tutorial/bootstrap path but is not yet a promoted acceptance route |
| GameLevels/scene model | **94%** | P6 accepted; retained scene/camera/render state and tutorial scene inputs are substantial |
| First-scene rendering | **97%** | Direct + atlas + GLES2 paths, rotated plist handling, recovered lookup/cache semantics, improved player preload resolution and capture tooling exist; P7 proof remains |
| Player spawn/input/action | **96%** | Spawn, motion, joystick, facing, live action playback, action buttons and reconstructed Controls evidence are in place; P8/P9 proof remains |
| Fresh-role tutorial route/UI | **89%** | `cp_lesson`, LoadingLayer timing/fade, LessonOverlay/dialogue/progression and Attack entry are reconstructed; later fidelity and live combat remain |
| SceneActions runtime | **89%** | Pair loading/playback, timed/spatial triggering, bound-object maps and direct/static geometry paths exist; some dynamic/global PRNG semantics remain bounded gaps |
| Persistence | **86%** | Minimal bounded P10 sidecar/autosave/restore is implemented; real process-restart proof remains |
| Enemy/combat slice | **58%** | Enemy identity/base HP, spawn plan, wave generation, health/death and callback guards exist; renderer-facing enemy actors, actions, hit/damage integration and completed combat loop remain |
| Modern Android runtime validation | **74%** | API 35/36 x86_64 and Android 15 16 KiB runtime coverage exist; API 35/36 arm64 physical-device proof remains |
| Static/native evidence and reproducibility | **99%** | Extensive Ghidra/unidbg/device-capture, exact Thumb/native cross-checks and fail-closed reconstruction tooling |
| ML/dispatch analysis pipeline | **95%** | v5.4 is the frozen champion, later models are gated by leakage-safe held-out metrics, and reusable mechanisms are migrating to `revenginstr` with parity checks |

## Immediate critical path

1. Re-import the user-owned OBB with the new structure-scanning importer and retain the resulting resource-root/import diagnostics.
2. Run one fresh contiguous real-device P7 capture and prove that the first offline scene visibly renders from the newly imported resource tree.
3. In the same runtime path, retain P8 evidence for the reconstructed player body/frame/action state.
4. Record one actual Android gesture that visibly changes player/control state and close P9.
5. Perform a real process restart and retain evidence that the implemented P10 sidecar restores the expected player motion state.
6. Materialize the first tutorial Attack-wave enemy actors from the already reviewed roster bindings and imported EnemyActions path.
7. Route real hit/damage/death state through `(wave_generation, slot)` and prove Attack -> next tutorial step without fabricated constants.
8. Validate the current build on API 35/36 `arm64-v8a` hardware and retain the release-gate device evidence.

## Current runtime state

`main` includes:

- a modern Gradle/CMake Android application with project-owned C++/JNI code;
- `armeabi-v7a` and `arm64-v8a` build targets;
- embedded **Lua 5.2.3**, matching the original runtime;
- deterministic decoding for transformed Lua/PNG/HPC/CSV resources;
- Android import flows for user-owned original APK and OBB data;
- two-pass OBB scanning that detects the logical resource root from recovered paths before importing the sibling resource tree;
- reconstructed Android -> JNI -> startup/lifecycle state;
- reconstructed `AppDelegate`, splash/scene-generation and `HelloWorld` -> `ManagementLayer` behavior;
- completed tracked P3 server-selection and P4/P5 ChooseHero -> enter-game transitions;
- a bounds-checked `HPData` replacement and completed P6 first scene-construction boundary;
- retained GameLevels scene/model state, recovered camera/parallax inputs and evidence-backed construction order;
- a live GameScene render queue;
- direct-file and sprite-frame asset request domains;
- Cocos-compatible plist/atlas parsing for formats 0-3, rotated-frame handling, texture resolution and atlas-frame staging;
- recovered player SpriteFrameCache preload-family behavior for `pNN_CC_01..09` default atlases and exact action-specific atlases;
- Java bitmap staging and transactional pixel revisions;
- GLES2 texture upload/cache and project-owned sprite drawing;
- recovered 1136x640 ExactFit geometry, anchor, scale, rotation, flip and bounded skewX behavior;
- reconstructed player spawn composition, body transform, fixed-clock motion and facing state;
- recovered Android `CCJoyStick` touch geometry, pointer ownership and dock visuals;
- live player action document loading and frame-by-frame `MeActionsSystem` playback;
- recovered player function/action gates, action buttons and selected action-state transitions;
- fresh-role `CreateCharacter` -> `cp_lesson.glData` tutorial bootstrap;
- recovered tutorial LoadingLayer 0.5 s switch boundary and 0.2 s hide fade;
- project-owned LessonOverlay/dialogue/progression state and imported tutorial UI asset loading;
- live tutorial Attack-gate providers for player facing, scene limits and reviewed enemy identity/base-health fields;
- fixed-tick first Attack-wave spawn planning and generation-safe roster identity;
- project-owned enemy health/death state matching the recovered current/max-health and `isDead()` behavior required by the tutorial slice;
- generation-safe tutorial enemy actor bindings ready for the renderer/action-system handoff;
- a reconstructed `GameSaveData::Encode/Decode` leaf codec validated against bounded native probes;
- an independent bounded P10 minimal save/autosave/restart-restore runtime;
- transactional `EnemyActionsData` structural parsing and expanded `TrapActionsData` reconstruction;
- a live SceneActions current-scene pair runtime with document/pixel staging and GLES submission;
- recovered SceneActions timed/spatial trigger contracts, event-ID mapping, bound-object maps, pair-level hit evaluation and direct/static geometry paths;
- recovered type-0 GameScene animation behavior for opacity and skewX loops;
- a repaired unidbg harness completing explicit `JNI_OnLoad` for the recorded dependency set;
- original-device-derived player/enemy runtime evidence;
- a normalized P-code ML/analysis testbed with conservative labels and leakage-resistant splits;
- static and runtime indirect-dispatch analysis, receiver/vtable inference and evidence provenance controls;
- broad migration/parity coverage for reusable reverse-engineering mechanisms in `revenginstr`;
- API 35/36 x86_64 lifecycle CI and Android 15 16 KiB runtime CI;
- an arm64 real-device evidence harness awaiting physical-device acceptance.

### Important current limitations

- P7 remains open because a complete accepted real-device imported-data first-scene capture has not yet been retained after the latest OBB/player-atlas fixes.
- P8/P9 implementations are deep, but the required contiguous proof of visible player + real touch-driven state change is still missing.
- P10 is implemented but has not yet been promoted because real process restart/restore evidence is still missing.
- Earlier reports of a missing player action/resource file must be rechecked after the new OBB root-scanning import path; archive-layout/import assumptions are no longer a valid reason by themselves to classify an asset as absent.
- The fresh-role tutorial path is substantially reconstructed but is not yet a promoted contiguous device acceptance route.
- The first tutorial Attack wave has reviewed spawn/identity/HP inputs and health/death semantics, but roster bindings are not yet visible/interactive EnemyObject-equivalent actors.
- Enemy action/resource selection, renderer-facing enemy frames, hitboxes/damage and combat completion remain the main early-playable gameplay boundary.
- SceneActions still has explicit evidence boundaries around some dynamic sprite/current-parent inputs and original process-global PRNG selection/order.
- ML/dispatch predictions are prioritization/evidence tools, not semantic proof; uncertain or conflicting results remain gated from runtime reconstruction.
- Actual API 35/36 `arm64-v8a` physical-device evidence is still required.

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
        +--------+----------------------+
        |                               |
        v                               v
 tracked existing-role path       fresh-role tutorial
        |                          cp_lesson.glData
        |                               |
        |                        LoadingLayer / fade
        |                               |
        |                         LessonOverlay/UI
        |                               |
        +---------------+---------------+
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
            +------------+-------------+
            |                          |
            v                          v
   minimal P10 autosave       tutorial Attack gate
            |                          |
            v                          v
 process restart/restore       roster + actor bindings
                                       |
                                       v
                         [next: visible enemy actors]
```

The runtime does **not** link against the original `libcocos2dcpp.so`.

## Resource recovery

Decode supported transformed assets locally from a legally obtained APK:

```bash
python3 tools/asset_decoder.py /path/to/com.hippiegame.nevergone.apk \
  --output build/decoded-assets
```

Recovered proprietary output should remain local and must not be committed.

The Android app also supports user-owned original APK/OBB import into app-private runtime storage. The OBB importer first scans the selected archive to locate the logical Never Gone resource root from already recovered scene/atlas paths, then imports the full sibling tree. It therefore supports archives rooted directly at game resources, conventional `assets/` archives and wrapped/nested layouts.

## Analysis strategy

The project does **not** attempt a blind line-by-line rewrite of the original `libcocos2dcpp.so`.

The strategy is dependency-driven and evidence-driven:

1. recover metadata, symbols, DEX/JNI relationships and script dependencies;
2. decode user-owned resources locally with reproducible tooling;
3. execute recovered startup scripts against a clean modern runtime;
4. reconstruct state machines at verified semantic boundaries instead of copying the old ABI;
5. convert callback/save/resource payloads into bounded project-owned models before attaching them to renderer state;
6. reconstruct binary readers only through proven field widths/order and stop when evidence runs out;
7. use bounded native execution, Ghidra slices and real-device probes where static analysis is insufficient;
8. use normalized P-code/ML only to rank or classify evidence candidates, never as a substitute for semantic proof;
9. preserve evidence provenance so natural observations, static inference and interventional probes cannot silently contaminate one another;
10. recreate only behavior required by the reachable offline path;
11. validate observable behavior on real Android hardware before declaring playable-path checkpoints complete.

Reusable reverse-engineering mechanisms are being extracted into [`Hanz0Hatt0r1/revenginstr`](https://github.com/Hanz0Hatt0r1/revenginstr). Nevergone remains the consumer/reference project and retains game-specific labels, catalogs, datasets and semantic evidence. See [`docs/ml/REVENGINSTR_MIGRATION.md`](docs/ml/REVENGINSTR_MIGRATION.md).

## ML and indirect-dispatch testbed

The repository includes a project-local normalized P-code/dispatch testbed under [`docs/ml/`](docs/ml/) and [`tools/ml/`](tools/ml/).

Current capabilities include:

- canonical high-P-code export with address/name leakage removed from model input;
- conservative seed/evidence labels with weighted provenance;
- deterministic train/validation/test splits grouped by normalized structure;
- structural/callgraph metrics and function-family mining;
- similarity/OOD/selective-confidence gates;
- evidence-aware production classification and accept-error routing;
- indirect-call pattern/slice analysis;
- static vtable prefix/catalog recovery and receiver-vtable slot resolution;
- parameter-relative, singleton, PHI/default-instance and structural receiver provenance recovery;
- mixed-receiver quarantine and safety finalization;
- runtime-trace/equivalence/consistency analysis;
- explicit natural/interventional evidence isolation;
- frozen unseen validation and drift auditing;
- generic `revenginstr` parity adapters for reusable mechanisms.

The current frozen Nevergone champion remains **v5.4** until a later candidate beats it under the same family-held-out, leakage-safe evaluation protocol. This tooling improves evidence discovery and prioritization but **does not advance the playable-path completion marker by itself**.

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
- APK/OBB import, OBB structure scanning and resource-root mapping;
- render-queue ordering and resource domains;
- direct-file and supported plist/atlas asset staging, including rotated frames and player preload-family resolution;
- GLES2 texture/cache/render contracts and sprite geometry;
- player spawn/motion/joystick/facing/action-button state;
- player action loading, timing, frame advancement, function/action gates and live playback;
- tutorial loading/bootstrap/fade/LessonOverlay/dialogue/progression state;
- tutorial Attack-wave spawn inputs, roster, actor bindings and live evidence gates;
- enemy health/death state and reviewed GameLevels enemy-record mapping;
- P10 minimal save/autosave/restart-restore regression;
- EnemyActions/TrapActions structural parsing;
- SceneActions document/frame/render/playback/timed/spatial trigger, bound-object and direct-geometry contracts;
- Ghidra/unidbg evidence tooling;
- ML corpus/classification/OOD/family/similarity/frozen-validation smoke tests;
- indirect-dispatch/vtable/receiver-provenance analysis smoke tests;
- `revenginstr` migration/parity gates;
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
- [x] OBB structure/root scanning implemented

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
- [x] OBB root-scan import fix for noncanonical archive layouts
- [x] player spawn/motion/action runtime foundations
- [x] recovered player atlas preload-family resolution
- [x] recovered Android joystick/facing/dock state
- [x] substantial exact-native Controls layout/callback/call-surface closure
- [x] P10 bounded minimal persistence implementation
- [ ] P7 retained real-device first-scene proof
- [ ] P8 retained real-device player proof
- [ ] P9 retained real-device visible-input proof
- [ ] P10 retained real process-restart/restore proof

### M5 — First tutorial / offline gameplay

- [x] fresh-role `CreateCharacter` -> `cp_lesson` bootstrap
- [x] LoadingLayer switch timing and hide fade
- [x] initial LessonOverlay/dialogue/progression runtime
- [x] live player action playback for the supported path
- [x] tutorial Attack-wave fixed-tick spawn plan
- [x] live player-facing and scene-limit providers
- [x] reviewed enemy ID/base-health mapping for the first wave
- [x] generation-safe tutorial roster and actor-binding identities
- [x] minimal enemy health/death contract
- [x] EnemyActions structural reconstruction
- [x] substantial TrapActions structural reconstruction
- [x] substantial bounded SceneActions runtime/trigger/geometry reconstruction
- [ ] renderer-facing enemy actor/action bridge
- [ ] real hit/damage/death callback path
- [ ] first completed enemy interaction / Attack lesson
- [ ] minimum combat systems beyond the first lesson
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

## Established findings

Among the established project facts:

- package: `com.hippiegame.nevergone`;
- original version: `1.0.9`;
- original native ABI: **`armeabi-v7a` only**;
- original native libraries: `libcocos2dcpp.so` and `libffmpeg.so`;
- the main library identifies its engine as **`cocos2d-2.1rc0-x-2.1.2`**;
- the embedded Lua runtime is **Lua 5.2.3**;
- the APK contains **107 transformed `.lua` modules** and all 107 can be decoded locally from a user-owned APK;
- the static Lua graph contains 107 modules, 104 resolved edges and 101 modules reachable from `Game.StartLua`;
- the original `libcocos2dcpp.so` retains roughly **26,875 defined function symbols**;
- the launcher is `com.hippiegame.nevergone.TJ_P_01` and the Java/JNI/native lifecycle path is mapped;
- `AppDelegate::AddAllSearchPath()` and its 59 child search directories are recovered;
- `GameLevels::LoadGameLevels()` section order and the first GameScene construction boundary are reconstructed;
- the original design space is 1136x640 with Cocos ExactFit behavior;
- imported direct-file and atlas-backed pixels can be staged and rendered through project-owned GLES2 code;
- `MeActionsSystem` initial state, frame advancement, completion behavior and a substantial live playback path are reconstructed;
- horizontal controls, joystick geometry and player motion/facing state are reconstructed for the supported path;
- fresh-role tutorial loading/order/timing into `cp_lesson` and LessonOverlay are reconstructed;
- first-wave tutorial scene limits, player facing, enemy identity/base-health and health/death rules have direct evidence-backed project-owned representations;
- `GameSaveData::Encode/Decode` byte transform is recovered, while P10 intentionally uses a bounded independent project-owned sidecar;
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

## Documentation

Key documents:

- [`docs/playable-path.md`](docs/playable-path.md) — primary delivery metric and P0-P10 definitions
- [`docs/roadmap.md`](docs/roadmap.md) — broader phased reconstruction plan
- [`docs/tutorial-loading-route.md`](docs/tutorial-loading-route.md) — fresh-role `cp_lesson`/LoadingLayer handoff
- [`docs/tutorial-lesson-overlay.md`](docs/tutorial-lesson-overlay.md) — reconstructed initial LessonOverlay
- [`docs/tutorial-attack-live-gate.md`](docs/tutorial-attack-live-gate.md) — live first Attack-wave evidence gate
- [`docs/tutorial-attack-wave-runtime.md`](docs/tutorial-attack-wave-runtime.md) — fixed-tick first-wave orchestration
- [`docs/tutorial-enemy-actor-bindings.md`](docs/tutorial-enemy-actor-bindings.md) — generation-safe enemy actor handoff boundary
- [`docs/enemy-health-death-contract.md`](docs/enemy-health-death-contract.md) — minimal enemy health/death behavior
- [`docs/minimal-save-restart-restore.md`](docs/minimal-save-restart-restore.md) — bounded P10 persistence slice
- [`docs/live-player-action-playback.md`](docs/live-player-action-playback.md) — recovered live player action playback path
- [`docs/player-input-runtime-evidence.md`](docs/player-input-runtime-evidence.md) — player input/runtime evidence
- [`docs/scene-action-pair-runtime.md`](docs/scene-action-pair-runtime.md) — SceneActions pair runtime
- [`docs/ml/README.md`](docs/ml/README.md) — normalized P-code/ML and static-dispatch testbed
- [`docs/ml/REVENGINSTR_MIGRATION.md`](docs/ml/REVENGINSTR_MIGRATION.md) — extraction of reusable analysis mechanisms
- [`docs/ml/PRODUCTION_CLASSIFIER_V57.md`](docs/ml/PRODUCTION_CLASSIFIER_V57.md) — latest constrained GAME meta-calibration experiment
- [`docs/evidence/unidbg-2026-10-10/README.md`](docs/evidence/unidbg-2026-10-10/README.md) — bounded native dynamic evidence

## Contributing

Useful contributions include function identification, Ghidra analysis, Java/JNI mapping, Cocos2d-x matching, script/resource-format research, bounded native experiments, clean-room runtime implementations, Android compatibility work and observable behavior comparison against the original game.

When documenting reconstructed behavior, include evidence whenever practical: binary hash/version, symbol/address, strings, xrefs, imports, call relationships or bounded runtime observations.

Do not commit original APK/OBB files, original native game binaries, copyrighted assets, user saves, decoded proprietary scripts, raw process memory or other private device data.

## Legal notice

This is an independent preservation and reverse-engineering project and is not affiliated with or endorsed by the original developers, publishers or rights holders of Never Gone.

The repository is intended to contain **original project code, documentation and reverse-engineering metadata only**. Users are expected to provide any required original game files from their own legally obtained copy.

All trademarks and copyrighted materials belong to their respective owners.
