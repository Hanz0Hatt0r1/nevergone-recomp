# Original Android APK baseline

This document records reproducible facts from the original Never Gone Android APK used for the initial reverse-engineering baseline. Proprietary binaries are **not** stored in this repository.

## Identity

| Item | Value |
| --- | --- |
| Package | `com.hippiegame.nevergone` |
| APK SHA-256 | `c8aaabfe28893998aaf891716147328245e16a043d95b7abdeac43ef6adc9cab` |
| ZIP entries | 580 |
| Uncompressed content | 50,699,438 bytes |
| DEX files | 1 (`classes.dex`) |
| Native ABI | `armeabi-v7a` only |

The binary Android manifest contains the original package name and launcher activity `com.hippiegame.nevergone.TJ_P_01`.

The original target SDK is from the legacy Android era and was low enough to trigger modern Android installation restrictions. The temporary compatibility patch used during research raised the target sufficiently to install on a current device, but this repository is aimed at a source-level rebuild rather than preserving that patched binary.

## Core files

| File | Size | SHA-256 |
| --- | ---: | --- |
| `classes.dex` | 3,921,596 | `f5694e22bc9b121e9ccb49827a41b8b65e9e33116997029c7a77a7ae3cc5aa7c` |
| `lib/armeabi-v7a/libcocos2dcpp.so` | 9,684,428 | `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e` |
| `lib/armeabi-v7a/libffmpeg.so` | 14,712,492 | `f5e8392d27b182671be5070bc7aa5772a8b19711af222833651faf8516eacdde` |

Both shared libraries are 32-bit little-endian ARM EABI5 ELF shared objects and are stripped.

## Engine identification

`libcocos2dcpp.so` contains the exact engine version string:

```text
cocos2d-2.1rc0-x-2.1.2
```

It also contains:

```text
$LuaVersion: Lua 5.2.3  Copyright (C) 1994-2013 Lua.org, PUC-Rio $
```

This is a major reduction in uncertainty: stock Cocos2d-x 2.1.2 code can be compared against the binary and separated from Never Gone-specific code instead of being reconstructed as unknown game code.

Other third-party code visible in the native symbol/string set includes protobuf, OpenSSL-era crypto/TLS code, Box2D, libpng, libtiff, WebP and other libraries that appear to have been linked into the main native library.

## Dynamic dependencies

`libcocos2dcpp.so` declares these `DT_NEEDED` entries:

```text
./obj/local/armeabi-v7a/libffmpeg.so
liblog.so
libz.so
libGLESv2.so
libdl.so
libstdc++.so
libm.so
libc.so
```

The first entry is notable because it embeds an Android NDK build-tree path instead of the normal soname `libffmpeg.so`. This was one of the compatibility defects corrected in the temporary modern-Android APK patch.

`libffmpeg.so` depends only on standard Android system libraries in its dynamic table (`libm.so`, `libz.so`, `libc.so`, `libdl.so`).

## Asset inventory

The APK contains 270 files below `assets/`, totaling 18,410,612 bytes.

Major asset types:

| Extension | Count |
| --- | ---: |
| `.lua` | 107 |
| `.png` | 97 |
| `.hpc` | 19 |
| `.mp3` | 18 |
| `.csv` | 14 |
| `.plist` | 8 |
| `.actdata` | 3 |
| `.wav` | 2 |
| `.txt` | 1 |
| `.mp4` | 1 |

Examples of configuration/data files include:

```text
assets/config/EndlessKeyDrop.csv
assets/config/chaperGate.csv
assets/config/backPackLimit.csv
assets/config/weaponResolveMaterial.csv
assets/config/ItemConfig.hpc
assets/config/SpecialItem_drop.csv
assets/Login/ALL_Loin.csv
assets/RandomName.csv
assets/TypesetText.csv
```

### Lua files

All 107 `.lua` files fail a simple plain-text/Lua-bytecode classification. None start with the normal Lua bytecode signature (`1B 4C 75 61`) and their printable-byte ratios are low. They should currently be treated as **encoded/obfuscated/encrypted data** rather than source Lua.

Examples:

```text
assets/Script/Game/StartLua.lua
assets/Script/Game/ClientRequire.lua
assets/Script/ShareLogic/require.lua
assets/Script/ShareLogic/Logic/MaterialLogic.lua
assets/Script/ShareLogic/Logic/MailLogic.lua
```

The first bytes form several recurring patterns, which suggests a repeatable transform rather than random encryption. Recovering the loader/decoder in native code is therefore a high-priority M2 task.

## Android/DEX surface

Game package classes found in `classes.dex` include:

- `com.hippiegame.nevergone.TJ_P_01`
- `com.hippiegame.nevergone.VideoView`
- `com.hippiegame.nevergone.GooglePlayIABPlugin`
- generated `R` classes and billing callback inner classes

The APK also embeds the legacy Cocos2d-x Android Java layer (`org.cocos2dx.lib.*`) and a gamepad bridge (`com.ngds.cocos.GamepadBridge`) plus `com.ngds.pad.*` service/binder classes.

The DEX contains strings `cocos2dcpp`, `ffmpeg` and `loadLibrary`, consistent with the two native libraries bundled in the APK.

## Native game surface

The main library exports many C++ symbols despite being stripped. RTTI reveals a large amount of the class model.

High-value game classes already identified include:

```text
AppDelegate
DataManager
LogicManager
GameSaveData
LoadingLayer
LoginScreen
GameSceneUI
EnemyObject
EquipManager
GuideManager
NPCManager
PaymentMgr
SockClient
SockServer
NGGamepadListener
ArenaLayer
ArenaWorld
BackpackUI
ChooseHero
DialogueUI
EndlessArena
FriendsWorld
GameLevels
LevelWorld
SkillObject
WeaponLayer
```

`AppDelegate` exposes the expected startup/lifecycle functions:

```text
AppDelegate::AddAllSearchPath()
AppDelegate::applicationDidFinishLaunching()
AppDelegate::applicationDidEnterBackground()
AppDelegate::applicationWillEnterForeground()
```

The next native-analysis step is to map `applicationDidFinishLaunching()` and identify the exact first game-owned scene/resource manager calls.

## Reproducing the baseline

Run the repository inventory scanner against a legally obtained original APK:

```bash
python3 tools/apk_inventory.py /path/to/nevergone.apk \
  --json analysis/apk-inventory.json \
  --markdown analysis/apk-inventory.md
```

After extracting `classes.dex` and the main native library, use:

```bash
python3 tools/dex_native_map.py classes.dex --markdown analysis/dex-native.md
python3 tools/elf_native_map.py libcocos2dcpp.so --markdown analysis/native-map.md
```

Generated reports should be treated as local research output unless they contain only non-proprietary metadata appropriate for committing.
