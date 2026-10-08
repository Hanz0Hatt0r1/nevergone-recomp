# Original APK analysis baseline

This document records metadata derived from the original Android APK used during the initial research pass. Proprietary game binaries and assets are not stored in this repository.

## Identity

- Package: `com.hippiegame.nevergone`
- Original version name: `1.0.9`
- Original APK SHA-256: `c8aaabfe28893998aaf891716147328245e16a043d95b7abdeac43ef6adc9cab`
- Original APK size: `31,660,790` bytes
- ZIP file entries: `580`
- DEX files: `classes.dex`

The compatibility experiment performed before creation of this repository raised the legacy `targetSdkVersion` from 17 to 24 and repaired a malformed native FFmpeg dependency. That patched APK is not part of the recompilation source tree.

## Original signing certificate

The unmodified baseline APK is signed with a self-issued X.509 certificate whose public metadata can be reproduced with `keytool -printcert -jarfile` or `tools/apk_inventory.py` when `keytool` is available.

- Subject/owner: `CN=xipishi, OU=xipishi, O=xipishi, L=zhuhai, ST=guangdong, C=CN`
- Issuer: `CN=xipishi, OU=xipishi, O=xipishi, L=zhuhai, ST=guangdong, C=CN`
- Serial number: `9d1a4a`
- Valid from: `Tue Feb 07 09:23:42 UTC 2017`
- Valid until: `Thu Jan 14 09:23:42 UTC 2117`
- Signature algorithm: `SHA256withRSA`
- Public key: `2048-bit RSA key`
- Certificate version: `3`
- SHA-1 fingerprint: `7F:11:60:D8:FA:92:65:49:74:7D:23:EE:54:7B:12:C4:84:E4:8C:89`
- SHA-256 fingerprint: `D5:74:95:5F:77:33:1A:DC:09:8C:A4:CE:A2:B6:1E:12:47:93:9F:42:F0:D2:40:99:01:33:B7:31:0C:C2:05:36`

Only certificate metadata is documented. No private signing material is present in the repository. Modified or recompiled builds must use a project/user-controlled signing key and therefore cannot be signature-compatible upgrades over the original package unless the original private key is independently available to its owner.

## Native libraries

The original APK contains only the `armeabi-v7a` ABI.

### `lib/armeabi-v7a/libcocos2dcpp.so`

- Size: `9,684,428` bytes
- SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`
- ELF: 32-bit little-endian ARM shared object
- Architecture: ARM EABI5
- Status: stripped in the normal debugging-symbol sense, but retains an unusually rich dynamic symbol table

Observed `DT_NEEDED` entries:

- `./obj/local/armeabi-v7a/libffmpeg.so`
- `liblog.so`
- `libz.so`
- `libGLESv2.so`
- `libdl.so`
- `libstdc++.so`
- `libm.so`
- `libc.so`

The first entry embeds a build-machine-relative path. A compatibility patch proved that replacing it with `libffmpeg.so` produces a structurally valid installable APK.

### `lib/armeabi-v7a/libffmpeg.so`

- Size: `14,712,492` bytes
- SHA-256: `f5e8392d27b182671be5070bc7aa5772a8b19711af222833651faf8516eacdde`
- ELF: 32-bit little-endian ARM shared object
- Architecture: ARM EABI5

Observed dependencies:

- `libm.so`
- `libz.so`
- `libc.so`
- `libdl.so`

## Engine identification

A native string in `libcocos2dcpp.so` identifies the engine build as:

```text
cocos2d-2.1rc0-x-2.1.2
```

Other evidence is consistent with the Cocos2d-x 2.x API family: `CCDirector`, `CCLayer`, `CCFileUtils`, `CCObject`, `CCBReader`, `CCEditBox`, and related classes are present by name.

The library also contains build-path evidence referencing an Android NDK r11b-era toolchain and old Cocos2d-x third-party libraries. These paths are useful for identifying dependency versions but should not be treated as required build paths for the recompilation.

## Asset inventory

The APK contains `270` files under `assets/`, totaling approximately `18.4 MB` uncompressed.

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
| `.mp4` | 1 |
| `.txt` | 1 |

Notable script paths include `assets/assets/Script/Game/StartLua.lua`, `ClientRequire.lua`, engine wrappers, public RPC/gateway code, UI code, and a large set of gameplay/data-manager modules.

## Lua payloads

All 107 `.lua` files fail the simple tests for either plain Lua source or standard Lua bytecode (`\x1bLua`).

Baseline statistics from `tools/lua_probe.py`:

- plain source: `0`
- standard Lua bytecode: `0`
- encoded/obfuscated: `107`
- unique 8-byte prefixes: `29`
- average first-block printable ratio: approximately `0.36`
- average sampled entropy: approximately `7.7 bits/byte`

This strongly suggests a custom transformation or packed representation rather than ordinary source files stored under misleading extensions.

The native startup code contains the literal path:

```text
assets/Script/Game/StartLua.lua
```

`CallLuaCalss::init()` resolves the path through `cocos2d::CCFileUtils` and then calls the embedded `luaL_loadfilex`. This establishes a concrete trace target for finding the transform that turns the APK payload into parser-readable Lua.

The native library also exports functions named `IsEncryptFile`, `EncryptMemory`, and `DecryptMemory`. Disassembly of `DecryptMemory` shows a simple byte transform, but currently observed call sites are in payment/network code. It is therefore only a candidate for further investigation and is **not** currently identified as the Lua decoder.

## Java / JNI observations

The DEX contains the package classes `com.hippiegame.nevergone.*`, Cocos2d-x Java glue under `org.cocos2dx.lib.*`, Google Play-era components, video support, and an in-app billing plugin.

Game-specific JNI exports currently identified from the native library are:

- `Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnFailed`
- `Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnPurchased`
- `Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnReceiveItemInfo`
- `Java_com_hippiegame_nevergone_GooglePlayIABPlugin_nativeOnRestore`

The remaining JNI exports are primarily Cocos2d-x renderer, touch, accelerometer, bitmap and helper functions. See `docs/native-analysis.md` and the output of `tools/native_symbol_map.py` for the exact address map.

## Compatibility constraints confirmed so far

- Original native code is only `armeabi-v7a`.
- A 64-bit-only Android device cannot execute the original native library.
- Native binaries were produced for old Android assumptions and are not a modern 16 KiB-page-size rebuild.
- Original target SDK is obsolete for direct installation on recent Android versions.
- Re-signing any modified APK breaks upgrade compatibility with the original developer signature.

These are reasons to reconstruct the source/build rather than accumulate binary patches.

## Reproducing this report

```bash
python3 tools/apk_inventory.py /path/to/original.apk \
  --json build/apk-inventory.json \
  --markdown build/apk-inventory.md

python3 tools/lua_probe.py /path/to/original.apk
```

When `keytool` is installed, the APK inventory also records public signing-certificate metadata and fingerprints. The generated files under `build/` should remain local unless they contain only non-proprietary metadata suitable for publication.
