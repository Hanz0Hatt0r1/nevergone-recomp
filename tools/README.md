# Analysis tools

These scripts are intended to operate on a user's own legally obtained original game files. They do not require proprietary binaries to be committed to the repository.

## Requirements

- Python 3.10+
- `readelf` for ELF metadata
- `strings` for version/build strings
- `c++filt` for C++ symbol demangling

On most Linux distributions these are provided by binutils.

## APK inventory

```bash
python3 tools/apk_inventory.py /path/to/com.hippiegame.nevergone.apk \
  --json build/apk-inventory.json \
  --markdown build/apk-inventory.md
```

The report contains hashes, file counts, asset-extension counts, native dependencies, Cocos strings, JNI exports and a simple classification of Lua-like files.

## Lua probe

```bash
python3 tools/lua_probe.py /path/to/com.hippiegame.nevergone.apk
```

This does not extract or redistribute scripts. It fingerprints `.lua` payloads, reports source/bytecode/encoded classifications, estimates sampled entropy and groups common headers.

## Native symbol map

Extract `libcocos2dcpp.so` locally from the APK, then run:

```bash
python3 tools/native_symbol_map.py /path/to/libcocos2dcpp.so \
  --csv build/native-symbols.csv \
  --markdown build/native-symbols.md
```

The generated CSV records function addresses, the raw ARM/Thumb symbol address, symbol size, mangled/demangled names, heuristic subsystem category and top-level class/namespace owner.

## Repository policy

Generated reports containing only hashes, addresses, names and other reverse-engineering metadata may be committed when useful. Do not commit original APKs, `.so` files, game assets, decoded proprietary scripts, or other copyrighted game data.
