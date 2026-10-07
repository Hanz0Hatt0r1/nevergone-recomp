# Never Gone Recomp

Experimental reverse-engineering and recompilation project for the Android version of **Never Gone**.

The long-term goal is to understand the original game runtime, reconstruct the game-specific native/script integration, and produce a maintainable build for modern Android — including `arm64-v8a` and 64-bit-only devices — without depending on the original obsolete Android toolchain.

> [!IMPORTANT]
> The project is in an early research/reconstruction stage. It is **not yet a playable recompilation**.

## Goals

- Reverse engineer the original Android APK and native libraries.
- Recover and document the Android → JNI → Cocos2d-x → Lua startup flow.
- Separate upstream engine/library code from Never Gone-specific code.
- Recover the encoded Lua pipeline and script module graph.
- Reconstruct game-specific native systems as maintainable source.
- Replace or stub obsolete online/platform integrations where required for preservation.
- Produce reproducible modern Android builds.
- Support `arm64-v8a` and modern Android runtime requirements.
- Keep offline game behavior as close to the original release as practical.

## Current findings

The initial research pass has already established several useful facts:

- package: `com.hippiegame.nevergone`;
- original version: `1.0.9`;
- original native ABI: **`armeabi-v7a` only**;
- native libraries: `libcocos2dcpp.so` and `libffmpeg.so`;
- the main library identifies its engine as **`cocos2d-2.1rc0-x-2.1.2`**;
- the APK contains one `classes.dex`;
- the APK contains **107 `.lua` files**, all of which appear encoded/obfuscated rather than plain Lua source or standard Lua bytecode;
- native startup code resolves `assets/Script/Game/StartLua.lua` and passes it toward the embedded Lua runtime;
- `libcocos2dcpp.so` retains roughly **26,875 defined function symbols**, despite lacking ordinary debug information;
- many game classes are available by name, including `GameScene`, `GameSceneUI`, `MEPlayer`, `EnemyObject`, `BattleManager`, `GameSaveData`, `DataManager`, and others;
- 22 JNI exports have been identified;
- the original native FFmpeg dependency contains a legacy build-machine path (`./obj/local/armeabi-v7a/libffmpeg.so`).

These findings make a dependency-first reconstruction practical and reduce the amount of blind manual decompilation required.

## Current status

| Area | Status |
| --- | --- |
| Original APK baseline / hashes | Done |
| Modern Android installation experiment | Done |
| Native library inventory | Done |
| Cocos2d-x version identification | Done — 2.1.2 family |
| Initial JNI map | Done |
| Automated native symbol classification | Done |
| Asset inventory | Done |
| Lua payload fingerprinting | Done |
| Lua decoder / recovered scripts | In progress |
| Java/DEX bootstrap map | Next |
| Ghidra subsystem map | Next |
| Buildable native reconstruction | Not started |
| Modern Gradle/CMake app | Not started |
| `arm64-v8a` build | Not started |
| Playable recompilation | Not started |

## Strategy

The project deliberately avoids starting with a full line-by-line rewrite of `libcocos2dcpp.so`.

The fastest path is currently:

1. recover the Android/JNI startup contract;
2. recover the Lua decoding/loading path and script module graph;
3. use the retained C++ symbol table to partition native code into Cocos2d-x, protobuf, Lua, third-party and game-specific subsystems;
4. compare engine code against the identified Cocos2d-x 2.1.2-era source;
5. reconstruct only the remaining game-specific native interfaces in dependency order;
6. create a modern Android/NDK build once those runtime contracts are understood;
7. restore offline gameplay incrementally, then add `arm64-v8a`/modern Android compatibility.

See **[`docs/roadmap.md`](docs/roadmap.md)** for the detailed development plan.

## Repository layout

```text
nevergone-recomp/
├── README.md
├── docs/
│   ├── apk-analysis.md
│   ├── native-analysis.md
│   └── roadmap.md
├── tools/
│   ├── README.md
│   ├── apk_inventory.py
│   ├── lua_probe.py
│   └── native_symbol_map.py
├── src/                  # reconstructed code (future)
├── android/              # modern Android wrapper (future)
├── cmake/                # native build support (future)
└── tests/                # behavior/format tests (future)
```

## Analysis tools

The first tooling is already usable and requires no proprietary files in the repository.

Generate a reproducible APK inventory:

```bash
python3 tools/apk_inventory.py /path/to/com.hippiegame.nevergone.apk \
  --json build/apk-inventory.json \
  --markdown build/apk-inventory.md
```

Fingerprint the Lua-like payloads:

```bash
python3 tools/lua_probe.py /path/to/com.hippiegame.nevergone.apk
```

Generate a categorized native symbol database:

```bash
python3 tools/native_symbol_map.py /path/to/libcocos2dcpp.so \
  --csv build/native-symbols.csv \
  --markdown build/native-symbols.md
```

See **[`tools/README.md`](tools/README.md)** for details.

## Original Android architecture

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
│       ├── embedded Lua runtime / bindings
│       ├── Google protobuf
│       ├── TinyXML
│       ├── resource / save / networking systems
│       └── UI / combat / scene code
│
├── libffmpeg.so
│
└── assets
    ├── encoded Lua scripts
    ├── PNG / plist UI and graphics data
    ├── HPC / CSV configuration data
    └── audio / video
```

A high-priority trace already shows `CallLuaCalss::init()` resolving `assets/Script/Game/StartLua.lua` through `CCFileUtils` and invoking `luaL_loadfilex`. The exact byte transform applied before the Lua parser sees the script remains under investigation.

## Major milestones

### M0 — Research baseline

- [x] APK documented
- [x] native libraries identified
- [x] engine version identified
- [x] initial JNI and symbol maps generated
- [x] Lua payloads fingerprinted

### M1 — Runtime map

- [ ] Java/DEX startup path documented
- [ ] Lua decoding/loading path recovered
- [ ] first script recovered reproducibly
- [ ] major native subsystem graph documented

### M2 — Resource/script pipeline

- [ ] all recoverable Lua modules decoded by tooling
- [ ] script dependency graph built
- [ ] HPC/ACTDATA formats investigated as required
- [ ] user-supplied asset import workflow defined

### M3 — First reconstructed executable

- [ ] modern Gradle/CMake project builds
- [ ] reconstructed native library loads
- [ ] Android lifecycle/JNI bridge works
- [ ] engine/runtime initialization begins without the original `libcocos2dcpp.so`

### M4 — Boot to UI

- [ ] graphics/input initialization works
- [ ] resource search paths work
- [ ] Lua startup executes
- [ ] initial title/login/menu flow appears

### M5 — Offline gameplay

- [ ] game scene starts
- [ ] player/enemy control works
- [ ] combat works
- [ ] save/load works
- [ ] audio/video works

### M6 — Modern Android target

- [ ] `arm64-v8a`
- [ ] Android 15/16+ validation
- [ ] 16 KiB-page-size-compatible native build
- [ ] reproducible release build

## Documentation

- **[`docs/apk-analysis.md`](docs/apk-analysis.md)** — original APK/native/assets baseline
- **[`docs/native-analysis.md`](docs/native-analysis.md)** — symbol counts, JNI addresses, high-value classes and Lua/native observations
- **[`docs/roadmap.md`](docs/roadmap.md)** — phased reconstruction plan and immediate priorities

## Contributing

Reverse-engineering findings are useful even before they become reconstructed source code. Helpful contributions include function identification, Ghidra analysis, Java/JNI mapping, Cocos2d-x matching, script/resource format research, extraction tools, Android compatibility work and behavior comparison against the original game.

When documenting reconstructed functions, include evidence whenever practical: binary hash/version, symbol/address, strings, xrefs, imports, call relationships or runtime observations.

Do not commit original APKs, native game binaries, copyrighted game assets or decoded proprietary script contents. Tools and reverse-engineering metadata should be sufficient for users to work from their own legally obtained copy.

## Legal notice

This is an independent preservation and reverse-engineering project and is not affiliated with or endorsed by the original developers, publishers, or rights holders of Never Gone.

The repository is intended to contain **original project code, documentation and reverse-engineering metadata only**. Users are expected to provide required original game files from their own legally obtained copy.

All trademarks and copyrighted materials belong to their respective owners.
