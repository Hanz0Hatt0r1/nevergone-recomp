# Development roadmap

This roadmap is ordered to minimize blind manual decompilation. The project should first recover all metadata and script-level behavior that can be extracted automatically, then reconstruct only the native code that remains necessary.

## Phase 0 — Baseline and preservation

Status: **mostly complete**

- [x] Preserve an original APK hash.
- [x] Verify that a compatibility-patched APK can install on a modern Android version.
- [x] Confirm the original package is `com.hippiegame.nevergone`.
- [x] Record original version `1.0.9`.
- [x] Record native ABIs and shared libraries.
- [x] Identify the engine family and exact embedded version string.
- [x] Create reproducible inventory tooling.
- [ ] Decode and commit a human-readable manifest summary.
- [ ] Record signing-certificate metadata for the original APK.

Exit condition: another contributor can reproduce the same baseline from their own APK.

## Phase 1 — Java/Android bootstrap map

Priority: **high**

- [ ] Decompile `classes.dex` with JADX and preserve a metadata-only class/lifecycle map.
- [x] Identify the launcher Activity (`com.hippiegame.nevergone.TJ_P_01`).
- [ ] Fully map native library loading (`System.loadLibrary`).
- [ ] Document Android lifecycle forwarding into Cocos2d-x.
- [x] Map the DEX native-method declarations against static `Java_*` exports.
- [x] Document `JNI_OnLoad` and the first native scene path.
- [ ] Resolve or classify the six `GamepadBridge` native declarations without static exports.
- [ ] Document Google Play / IAP integration and decide what should be stubbed or replaced.
- [ ] Produce `docs/android-bootstrap.md`.

Current native startup evidence is documented in `docs/jni-map.md` and `docs/startup-flow.md`.

Exit condition: startup from Android process creation to `JNI_OnLoad` and the first native scene is documented end-to-end.

## Phase 2 — Lua/resource recovery

Priority: **critical path — major blockers resolved**

The APK contains 107 files with a `.lua` extension. They are ordinary Lua source passed through the same reversible file transform used for `.png`, `.hpc`, and `.csv` game assets.

- [x] Count and fingerprint all Lua-like payloads.
- [x] Confirm that the archived files are transformed rather than standard Lua source/bytecode.
- [x] Locate the native startup path that resolves `assets/Script/Game/StartLua.lua` and passes it to `luaL_loadfilex`.
- [x] Trace the file-reader path used by Lua into `CCFileUtilsAndroid::getFileData()`.
- [x] Recover the exact `cocos2d::Decode` algorithm and key used by the shipped build.
- [x] Recover `StartLua.lua` as a proof of concept.
- [x] Implement a standalone local asset decoder/import tool.
- [x] Validate all 107 decoded Lua payloads as text (99 UTF-8-compatible, 8 GB18030-compatible).
- [x] Validate all transformed PNG/HPC/CSV assets handled by the same decoder (237 total transformed assets, 0 format-validation failures).
- [ ] Syntax-check all recovered scripts with the matching Lua 5.2.3 runtime.
- [x] Build a static module dependency graph beginning with `Game.StartLua`.
- [x] Map the Lua-to-C++ binding/API surface used by reachable client scripts.
- [ ] Determine schemas/consumers for decoded `.hpc` configuration files.

The static Lua graph contains 107 modules and 104 resolved edges. `Game.StartLua` directly loads `Game.ClientRequire` and `ShareLogic.require`; 101 modules are reachable from that startup graph. Four absent shared/server-style module roots are referenced only from an isolated `Game.Logic.HpGame` subgraph and are not currently considered a normal client-startup blocker.

The Lua/native cross-check identifies 117 native-shaped or ELF-evidenced API candidates in the 101 reachable modules. 108 have direct native evidence: 106 dynamic-symbol matches, one exact native string match (`Lua_GetDeviceUUID`), and one native-class match (`ProtoRPC:new`). Only seven native-facing calls are used directly by `Game.StartLua`, giving the recompilation a small first bootstrap contract.

The earlier native functions named `IsEncryptFile`, `EncryptMemory`, and `DecryptMemory` are not the Lua asset decoder. The actual transformation is the `cocos2d::Decode` call in `CCFileUtilsAndroid::getFileData()`.

Exit condition: all recoverable client scripts can be deterministically decoded, validated, dependency-indexed, and their native API requirements are known from a user-provided original APK.

## Phase 3 — Native symbol and subsystem map

Priority: **high**

The main native library retains a very large dynamic symbol table, which changes the strategy significantly: named functions and classes should be harvested before decompiling individual routines.

- [x] Build a repeatable symbol classifier.
- [x] Identify JNI exports.
- [x] Identify major game classes from C++ symbols.
- [x] Map `AppDelegate::applicationDidFinishLaunching()` into `HelloWorld` and `ManagementLayer`.
- [x] Recover `AppDelegate::AddAllSearchPath()` and its 59 child search directories.
- [ ] Generate a full CSV symbol database for each known binary build and retain normalized metadata.
- [ ] Import names into Ghidra and create a project-specific data type archive.
- [ ] Separate Cocos2d-x, Lua, protobuf, TinyXML, standard-library, and game code with stronger version/source matching.
- [ ] Map constructors/destructors/vtables for top-priority game classes.
- [ ] Map global singleton accessors and startup ordering beyond the initial scene.
- [ ] Produce subsystem graphs for scene, player, combat, save data, networking and UI.
- [ ] Map the registration/bootstrap implementation behind the Lua-facing API names and resolve the nine unmatched native-shaped calls.

Initial high-value classes include:

- `GameScene`
- `GameSceneUI`
- `MEPlayer`
- `EnemyObject`
- `BattleManager`
- `DataManager`
- `GameSaveData`
- `GuideManager`
- `MEWarehouse`
- `AppParameters`
- `ManagementLayer`

Exit condition: native game code is partitioned into understandable subsystems and the next functions to reconstruct are selected by dependency rather than guesswork.

## Phase 4 — Engine reconstruction baseline

Priority: **high — needed for first real boot**

A native string identifies the engine as `cocos2d-2.1rc0-x-2.1.2`. Use that revision/version family as the first comparison target.

- [ ] Obtain/build the matching Cocos2d-x revision independently.
- [ ] Match known engine symbols and layouts against the shipped library.
- [ ] Document local/vendor modifications, including the custom asset decode path.
- [ ] Recreate the minimum engine-facing headers needed by reconstructed game code.
- [ ] Decide whether the final project should retain the old engine internally or port behavior to a newer compatibility layer.

Exit condition: upstream engine code can be distinguished from Never Gone-specific code with high confidence.

## Phase 5 — Modern build skeleton

Status: **initial shell implemented**

- [x] Create Gradle project.
- [x] Create CMake native build.
- [x] Add a Java/JNI bootstrap that loads project-owned native code.
- [x] Add an empty native game module that can be loaded successfully.
- [x] Configure `armeabi-v7a` as a behavior-comparison target.
- [x] Configure `arm64-v8a` as the primary modern target.
- [x] Add runtime ABI/pointer-width/page-size diagnostics.
- [x] Add a pinned Lua 5.2.3 CMake dependency with an offline source override.
- [ ] Validate a full build/install of the clean shell on both ABI classes.
- [ ] Add explicit 16 KiB page-size build validation.
- [ ] Integrate the user-owned asset import/preparation workflow with the recovered decoder.
- [ ] Set up CI for host-side tools and Android compilation.

The clean shell lives under `android/`. It targets current Android, uses project-owned C++ code, and does not link the original `libcocos2dcpp.so` or redistribute original assets.

Exit condition: a clean source checkout can build and launch a stub application on modern Android.

## Phase 6 — Boot path reconstruction

Priority: **current implementation target**

- [x] Integrate the matching Lua 5.2.3 runtime into the modern native module.
- [x] Register initial implementations/stubs for the seven native-facing calls used directly by `Game.StartLua`.
- [x] Recreate `CAddDoString` module-to-file loading for a locally imported decoded script tree.
- [x] Add a non-proprietary Lua smoke test to the Android bootstrap.
- [ ] Recreate application initialization / `AppDelegate` behavior required before script startup.
- [ ] Initialize rendering and input.
- [ ] Recreate filesystem/search-path behavior beyond the initial script root.
- [ ] Import/load decoded script/config resources supplied by the user on-device.
- [ ] Execute the real `Game.StartLua` without the original native library.
- [ ] Recreate additional native bindings on demand from the reachable startup graph.
- [ ] Reach the original initial UI flow (`HelloWorld` → `ManagementLayer`).

The four UI-facing startup functions currently log and return success. Their original argument/dispatch behavior has been identified from Thumb disassembly so the stubs can be replaced without changing the Lua API.

Exit condition: recompilation reaches the title/login/menu flow without original native code.

## Phase 7 — Offline gameplay restoration

The preservation target should prefer offline functionality over recreating obsolete external services.

- [ ] Character selection.
- [ ] Scene loading.
- [ ] Player controller.
- [ ] Enemy logic.
- [ ] Combat and damage.
- [ ] Equipment/inventory.
- [ ] Save/load.
- [ ] Audio/video playback.
- [ ] Tutorials and progression.
- [ ] Replace or stub unavailable network-only systems where necessary.

Exit condition: a representative offline gameplay loop is functional.

## Phase 8 — Modern Android release target

- [ ] `arm64-v8a` release build.
- [ ] Android 15/16+ runtime validation.
- [ ] 16 KiB page-size validation.
- [ ] Modern storage/audio/input behavior.
- [ ] Crash-free lifecycle resume/suspend testing.
- [ ] Reproducible build documentation.
- [ ] User-supplied asset import workflow.

Exit condition: the project produces a maintainable modern Android build without redistributing proprietary game assets or original binaries.

## Immediate next tasks

1. Integrate the local user-owned asset decoder/import workflow with the Android app-private files tree.
2. Execute the real `Game.StartLua` under the reconstructed Lua 5.2.3 runtime and capture the first missing binding/class.
3. Implement the missing bindings iteratively using `docs/lua-native-api-map.md` as the priority list.
4. Finish the Java/Android bootstrap map from `TJ_P_01` through library loading and lifecycle forwarding.
5. Trace the registration path for the nine native-shaped Lua names not confirmed by dynamic symbols/string evidence.
6. Expand the native subsystem map around `ManagementLayer`, `DataManager`, `LogicManager`, and `GameSaveData`.
