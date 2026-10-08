# Analysis tools

These scripts are intended to operate on a user's own legally obtained original game files. They do not require proprietary binaries to be committed to the repository.

## Requirements

- Python 3.10+
- `readelf` for ELF metadata and JNI export inspection
- `nm` for dynamic-symbol matching
- `strings` for version/build/registration strings
- `c++filt` for C++ symbol demangling
- `llvm-objdump` for ARMv7/Thumb PIC-string recovery

On most Linux distributions the GNU binary utilities are provided by binutils. `llvm-objdump` is normally provided by LLVM/Clang.

## APK inventory

```bash
python3 tools/apk_inventory.py /path/to/com.hippiegame.nevergone.apk \
  --json build/apk-inventory.json \
  --markdown build/apk-inventory.md
```

The report contains hashes, file counts, asset-extension counts, native dependencies, Cocos strings, JNI exports and a simple classification of Lua-like files. When `keytool` is available it also records public signing-certificate metadata and fingerprints.

## Android manifest summary

Decode the APK's binary `AndroidManifest.xml` without apktool/aapt and emit metadata-only JSON/Markdown:

```bash
python3 tools/apk_manifest_summary.py /path/to/com.hippiegame.nevergone.apk \
  --json build/original-manifest.json \
  --markdown build/original-manifest.md
```

The self-contained parser records package/version/SDK metadata, GLES requirements, permissions, screen support, launcher activities, application metadata and Android components. It deliberately leaves unresolved resource references as numeric IDs and does not retain proprietary manifest bytes or resource tables. The known baseline is committed as `docs/original-manifest.md`.

## Lua probe

```bash
python3 tools/lua_probe.py /path/to/com.hippiegame.nevergone.apk
```

This does not extract or redistribute scripts. It fingerprints `.lua` payloads, reports source/bytecode/encoded classifications, estimates sampled entropy and groups common headers.

## Asset decoder

The original game's file layer applies a small reversible transform to `.png`, `.hpc`, `.csv`, and `.lua` assets. The transform has been recovered from `CCFileUtilsAndroid::getFileData()` and `cocos2d::Decode()`.

Validate the known encoded assets without writing decoded proprietary content:

```bash
python3 tools/asset_decoder.py /path/to/com.hippiegame.nevergone.apk
```

Expected baseline for the known APK:

```text
processed: 237
.csv: 14
.hpc: 19
.lua: 107
.png: 97
validation failures: 0
```

To create a local decoded tree for research/runtime testing:

```bash
python3 tools/asset_decoder.py /path/to/com.hippiegame.nevergone.apk \
  --output build/decoded-assets
```

Decoded assets/scripts remain copyrighted game data and must not be committed.

## Lua dependency map

Build a metadata-only graph directly from an original APK. Encoded Lua files are decoded in memory and their source text is not written:

```bash
python3 tools/lua_dependency_map.py /path/to/com.hippiegame.nevergone.apk \
  --json build/lua-dependencies.json \
  --markdown build/lua-dependencies.md
```

The known APK baseline is:

```text
107 modules
108 static loader references
104 resolved references
4 unresolved references
101 modules reachable from Game.StartLua
6 modules outside the static startup graph
```

The analyzer recognizes direct string-literal calls to `require`, `CAddDoString`, and `dofile`, strips ordinary Lua comments, and resolves exact or unambiguous suffix module names. It is a static lower bound rather than a full Lua runtime tracer.

See `docs/lua-dependency-map.md` for the current graph findings.

## Lua to native API map

Cross-reference calls made by startup-reachable Lua modules against the original native library:

```bash
python3 tools/lua_native_api_map.py /path/to/com.hippiegame.nevergone.apk \
  --json build/lua-native-api.json \
  --markdown build/lua-native-api.md
```

When APK input is used, the script automatically reads `lib/armeabi-v7a/libcocos2dcpp.so` from the archive. For an already-decoded Lua tree, pass the ELF with `--elf`.

Known APK baseline:

```text
107 Lua modules
101 startup-reachable modules analyzed
117 native-shaped/evidenced API candidates
108 candidates confirmed by native ELF evidence
106 dynamic-symbol matches
1 binary-string registration/name match
1 native-class symbol match
9 native-shaped names still unconfirmed
```

The report is metadata-only and does not emit script bodies. It filters Lua-owned functions/tables so the output is focused on reconstruction targets. See `docs/lua-native-api-map.md` for the current startup contract and implementation order.

## Native symbol map

Extract `libcocos2dcpp.so` locally from the APK, then run:

```bash
python3 tools/native_symbol_map.py /path/to/libcocos2dcpp.so \
  --csv build/native-symbols.csv \
  --markdown build/native-symbols.md
```

The generated CSV records function addresses, the raw ARM/Thumb symbol address, symbol size, mangled/demangled names, heuristic subsystem category and top-level class/namespace owner.

## Thumb PIC string map

Many ARMv7 functions construct string constants through a literal-pool load followed by `add <reg>, pc`. Recover these references directly from a named function:

```bash
python3 tools/pic_string_map.py /path/to/libcocos2dcpp.so \
  'AppDelegate::AddAllSearchPath()' \
  --markdown build/add-search-paths-strings.md
```

For `AppDelegate::AddAllSearchPath()` the known binary yields 61 references: 59 child resource directories plus `assets` and the `%s/%s` formatting string. This is useful for reproducing native string evidence without requiring a shared Ghidra database.

## DEX native method map

Extract `classes.dex` locally, then run:

```bash
python3 tools/dex_native_map.py /path/to/classes.dex \
  --json build/dex-native.json \
  --markdown build/dex-native.md
```

This parser does not decompile Java bytecode. It reads the DEX metadata structures needed to enumerate methods carrying `ACC_NATIVE`, including their declaring class and expected static JNI symbol prefix.

For the known original Never Gone APK it finds 27 native method declarations.

## JNI cross-check

Compare DEX declarations with one or more bundled ELF libraries:

```bash
python3 tools/jni_crosscheck.py /path/to/classes.dex \
  /path/to/libcocos2dcpp.so \
  /path/to/libffmpeg.so \
  --json build/jni-crosscheck.json \
  --markdown build/jni-crosscheck.md
```

The known original APK produces this baseline:

```text
27 DEX native declarations
21 Java_* ELF exports
21 matched declarations
6 declarations without a static export
```

The six unmatched declarations are the methods on `com.ngds.cocos.GamepadBridge`. A missing static `Java_*` export is not automatically an error: it can also indicate `RegisterNatives`, a missing vendor library, or unreachable integration code. Use the report to select targets for manual disassembly/JADX analysis rather than treating it as a final diagnosis.

Add `--fail-on-missing` when using the tool in a validation workflow where any unmatched native declaration should cause a non-zero exit code.

## Repository policy

Generated reports containing only hashes, addresses, names and other reverse-engineering metadata may be committed when useful. Do not commit original APKs, `.so` files, game assets, decoded proprietary scripts, or other copyrighted game data.
