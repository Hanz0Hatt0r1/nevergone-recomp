# Never Gone Recomp

Experimental reverse-engineering and recompilation project for the Android version of **Never Gone**.

The long-term goal is to understand the original game executable, reconstruct its native code and Android integration, and produce a maintainable build that can run on modern Android devices — including 64-bit-only hardware — without depending on the original obsolete Android toolchain.

> [!IMPORTANT]
> This project is in a very early research stage. It is **not yet a playable recompilation**.

## Goals

- Reverse engineer the original Android APK and native libraries.
- Document the game's startup flow, JNI interface, engine integration, and resource loading.
- Reconstruct the native game code into a buildable source tree.
- Replace or update obsolete Android-specific components.
- Restore compatibility with current Android versions.
- Add native `arm64-v8a` support.
- Keep game behavior as close to the original release as practical.
- Produce a reproducible Android build using a modern NDK/CMake toolchain.

## Why this project exists

The original Android release was built for an older generation of Android devices and toolchains. Modern Android versions increasingly restrict or remove compatibility with software of that era.

Initial investigation of the original APK found several important compatibility issues:

- the application originally targets an old Android API level;
- native game code is provided only for **32-bit ARM (`armeabi-v7a`)**;
- the game uses native C/C++ code through JNI;
- the native executable is based on **Cocos2d-x**;
- the package contains an old **FFmpeg** integration;
- one native dependency referenced FFmpeg using a build-machine path rather than a normal Android shared-library name;
- future devices that are 64-bit-only cannot run the original ARMv7 executable directly.

A minimal APK compatibility patch has already demonstrated that the game can still be installed on a modern Android system. That patch is only a temporary compatibility measure; this repository aims at a proper source-level reconstruction.

## Current status

| Area | Status |
| --- | --- |
| Original APK identified | Done |
| Modern Android installation test | Done |
| Basic manifest compatibility investigation | Done |
| Native library inventory | In progress |
| ELF analysis | Planned / early research |
| JNI mapping | Planned |
| Cocos2d-x version identification | Planned |
| Resource format analysis | Planned |
| Decompiled/reconstructed game code | Not started |
| Buildable native project | Not started |
| `armeabi-v7a` recompilation | Not started |
| `arm64-v8a` port | Not started |
| Playable recompilation | Not started |

## Original Android architecture

The original game is expected to follow roughly this structure:

```text
Android application
│
├── Java / DEX bootstrap
│   └── JNI bridge
│
├── native game library
│   └── libcocos2dcpp.so
│       ├── game code
│       ├── Cocos2d-x
│       ├── rendering / audio
│       └── resource loading
│
├── FFmpeg native library
│
└── game assets / data
```

One of the first milestones is to turn this high-level view into a documented map of actual Java classes, JNI exports, imported native functions, subsystems, and game-specific code.

## Recompilation strategy

The project will proceed incrementally rather than trying to recreate the whole executable at once.

### 1. APK inventory

- decode `AndroidManifest.xml`;
- inspect DEX/Java bootstrap code;
- inventory assets and resource containers;
- identify every bundled native library;
- record hashes and metadata for known original binaries.

### 2. Native executable analysis

Analyze `libcocos2dcpp.so` and related libraries:

- ELF headers and sections;
- imports and exports;
- JNI symbols;
- strings and RTTI;
- C++ class relationships;
- engine/game boundary;
- initialization and shutdown paths;
- rendering, input, audio, networking, save data, and resource APIs.

### 3. Engine identification

Determine the exact or closest matching Cocos2d-x revision and third-party dependencies used by the game.

Matching the original engine version should allow known engine code to be separated from game-specific functions and substantially reduce the amount of code that needs to be reconstructed manually.

### 4. Game-code reconstruction

Recreate game-specific systems as compilable C/C++ code while continuously comparing behavior against the original executable.

Early priorities:

- application startup;
- scene creation;
- filesystem/resource access;
- input;
- rendering;
- audio;
- save/configuration handling;
- menu flow;
- gameplay initialization.

### 5. Modern Android runtime

Create a modern Android project using Gradle + CMake/NDK and progressively replace the legacy Android bootstrap.

Initial architecture targets:

```text
armeabi-v7a   compatibility/debug target
arm64-v8a     primary modern Android target
```

### 6. Validation

Behavior will be checked against the original version using reproducible tests where possible:

- startup sequence;
- resource loading;
- scene transitions;
- UI behavior;
- save-data compatibility;
- gameplay logic;
- rendering output;
- audio behavior.

## Planned repository layout

The structure will evolve as research progresses. A likely layout is:

```text
nevergone-recomp/
├── README.md
├── docs/
│   ├── apk-analysis.md
│   ├── native-analysis.md
│   ├── jni-map.md
│   ├── resource-formats.md
│   └── roadmap.md
├── tools/
│   ├── apk/
│   ├── elf/
│   └── ghidra/
├── src/
│   ├── game/
│   ├── platform/
│   └── reconstructed/
├── android/
├── cmake/
└── tests/
```

## Tools

Expected tooling includes:

- Ghidra
- JADX
- apktool
- Android SDK / NDK
- CMake
- LLVM/Clang
- `readelf`, `objdump`, `nm`, and related ELF utilities
- Python scripts for binary/resource analysis
- ADB / logcat for runtime comparison

Additional project-specific tools will be added to `tools/` as formats and workflows become understood.

## Milestones

### M0 — Research baseline

- APK documented
- native libraries identified
- Android entry points mapped
- initial compatibility findings documented

### M1 — Native map

- JNI interface documented
- major C++ subsystems identified
- engine/library code separated from likely game code

### M2 — Resource pipeline

- resource containers understood
- extraction tooling available
- runtime asset-loading behavior documented

### M3 — First reconstructed executable

- modern Android project builds
- reconstructed native library loads
- application reaches an initial engine/game entry point

### M4 — Boot to menu

- graphics initialization works
- resources load
- main menu is functional

### M5 — Gameplay

- game scene starts
- input, rendering, audio, and core gameplay systems function

### M6 — Modern Android release target

- `arm64-v8a` build
- modern Android compatibility
- reproducible build process
- documented testing against the original game

## Contributing

Reverse-engineering findings are useful even when they do not immediately translate into source code.

Helpful contributions include:

- function identification and naming;
- Ghidra analysis;
- JNI documentation;
- Cocos2d-x version identification;
- resource format documentation;
- extraction/conversion tools;
- Android compatibility research;
- build-system work;
- behavioral comparisons with the original game.

When documenting reconstructed functions, include evidence whenever possible: addresses, strings, xrefs, imports, call relationships, or runtime observations.

## Legal notice

This is an independent preservation and reverse-engineering project and is not affiliated with or endorsed by the original developers, publishers, or rights holders of Never Gone.

The repository is intended to contain **original project code, documentation, and reverse-engineering metadata only**. Proprietary game assets and copyrighted original binaries should not be committed to the repository.

Users are expected to provide any required original game files from their own legally obtained copy.

All trademarks and copyrighted materials belong to their respective owners.
