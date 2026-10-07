# Development roadmap

This roadmap is ordered to minimize blind manual decompilation. The project should first recover all metadata and script-level behavior that can be extracted automatically, then reconstruct only the native code that remains necessary.

## Phase 0 — Baseline and preservation

Status: **mostly complete**

- [x] Preserve an original APK hash.
- [x] Verify that a compatibility-patched APK can install on a modern Android version.
- [x] Confirm the original package is `com.hippiegame.nevergone`.
- [x] Record original version `1.0.9`.
- [x] Record native ABIs and shared libraries.
- [x] Identify the engine family and approximate version.
- [x] Create reproducible inventory tooling.
- [ ] Decode and commit a human-readable manifest summary.
- [ ] Record signing-certificate metadata for the original APK.

Exit condition: another contributor can reproduce the same baseline from their own APK.

## Phase 1 — Java/Android bootstrap map

Priority: **high**

- [ ] Decompile `classes.dex` with JADX.
- [ ] Identify the launcher Activity and application class.
- [ ] Map native library loading (`System.loadLibrary`).
- [ ] Document lifecycle forwarding into Cocos2d-x.
- [ ] Document Google Play / IAP integration and decide what should be stubbed or replaced.
- [ ] Produce `docs/android-bootstrap.md`.

Exit condition: startup from Android process creation to `JNI_OnLoad` and the first native scene is documented.

## Phase 2 — Lua recovery

Priority: **critical path**

The APK contains 107 files with a `.lua` extension. None are plain Lua source or standard Lua bytecode in the original archive. Recovering these scripts could restore a large part of game logic without reconstructing equivalent C++ manually.

- [x] Count and fingerprint all Lua-like payloads.
- [x] Confirm that they are encoded/obfuscated rather than standard Lua source/bytecode.
- [x] Locate the native startup path that resolves `assets/Script/Game/StartLua.lua` and passes it to `luaL_loadfilex`.
- [ ] Trace the file-reader path used by `luaL_loadfilex`.
- [ ] Determine where asset bytes are transformed before the Lua parser sees them.
- [ ] Recover one script as a proof of concept.
- [ ] Implement a standalone decoder/extractor.
- [ ] Batch-validate all 107 recovered scripts with a Lua parser.
- [ ] Build a module dependency graph beginning with `StartLua.lua` and `ClientRequire.lua`.

Note: native functions named `IsEncryptFile`, `EncryptMemory` and `DecryptMemory` exist, but current xrefs place the observed `DecryptMemory` calls in payment/network code. They must not be assumed to be the Lua decoder without further evidence.

Exit condition: all recoverable Lua scripts can be deterministically decoded from a user-provided original APK.

## Phase 3 — Native symbol and subsystem map

Priority: **high**

The main native library retains a very large dynamic symbol table, which changes the strategy significantly: named functions and classes should be harvested before decompiling individual routines.

- [x] Build a repeatable symbol classifier.
- [x] Identify JNI exports.
- [x] Identify major game classes from C++ symbols.
- [ ] Generate a full CSV symbol database for each known binary build.
- [ ] Import names into Ghidra and create a project-specific data type archive.
- [ ] Separate Cocos2d-x, Lua, protobuf, TinyXML, standard-library, and game code.
- [ ] Map constructors/destructors/vtables for top-priority game classes.
- [ ] Map global singleton accessors and startup ordering.
- [ ] Produce subsystem graphs for scene, player, combat, save data, networking and UI.

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

Exit condition: native game code is partitioned into understandable subsystems and the next functions to reconstruct are selected by dependency rather than guesswork.

## Phase 4 — Engine reconstruction baseline

Priority: **medium/high**

A native string identifies the engine as `cocos2d-2.1rc0-x-2.1.2`. Use that branch/version as the first comparison target.

- [ ] Obtain/build the matching Cocos2d-x revision independently.
- [ ] Match known engine symbols and layouts against the shipped library.
- [ ] Document local/vendor modifications.
- [ ] Recreate the minimum engine-facing headers needed by reconstructed game code.
- [ ] Decide whether the final project should retain the old engine internally or port behavior to a newer compatibility layer.

Exit condition: upstream engine code can be distinguished from Never Gone-specific code with high confidence.

## Phase 5 — Modern build skeleton

Priority: **after Phases 1–4 establish interfaces**

- [ ] Create Gradle project.
- [ ] Create CMake native build.
- [ ] Add Android lifecycle/JNI bootstrap.
- [ ] Add an empty native game module that can be loaded successfully.
- [ ] Support `armeabi-v7a` as a behavior-comparison target.
- [ ] Add `arm64-v8a` as the primary modern target.
- [ ] Add 16 KiB page-size-compatible linking/build settings.
- [ ] Set up CI for host-side tools and Android compilation.

Exit condition: a clean source checkout can build and launch a stub application on modern Android.

## Phase 6 — Boot path reconstruction

- [ ] Recreate application initialization.
- [ ] Initialize rendering and input.
- [ ] Recreate filesystem/search-path behavior.
- [ ] Load decoded script/config resources supplied by the user.
- [ ] Reach Lua `StartLua` execution.
- [ ] Reach the original initial UI flow.

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

1. Trace the bytes read by `luaL_loadfilex` back through the file reader and recover the first Lua script.
2. Decompile `classes.dex` and document the Android/JNI startup path.
3. Generate/import the native symbol database into Ghidra and prioritize startup/resource-loading functions.
4. Once those interfaces are stable, create the Gradle/CMake skeleton rather than guessing at the original runtime contract.
