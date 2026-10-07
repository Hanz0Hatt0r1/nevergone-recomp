# Asset encoding and decoding

The custom asset transform used by the original Android build has been recovered from `cocos2d::CCFileUtilsAndroid::getFileData()`.

This closes the main uncertainty around the files stored with `.lua`, `.hpc`, `.csv`, and `.png` extensions in the APK: they are not compressed Lua bytecode or an unknown container format. They are ordinary file contents passed through a small reversible byte transform.

## Where decoding happens

The shipped `libcocos2dcpp.so` contains these functions:

```text
cocos2d::Encode(unsigned char*, int, unsigned char*, unsigned int)
cocos2d::Decode(unsigned char*, int, unsigned char*, unsigned int)
cocos2d::CCFileUtilsAndroid::getFileData(char const*, char const*, unsigned long*)
```

`CCFileUtilsAndroid::getFileData()` normalizes the requested filename and checks for the following extension substrings:

```text
.png
.hpc
.csv
.lua
```

When one of them is present, it allocates a replacement buffer and calls:

```cpp
cocos2d::Decode(encoded, size, decoded, 1);
```

The original encoded buffer is then freed and the decoded buffer is returned to the caller.

This means the transform is below Lua/config/image parsing in the file layer rather than being a Lua-specific loader.

## Recovered algorithm

The ARMv7 implementation of `Decode` is equivalent to:

```cpp
void Decode(const uint8_t* input, size_t size, uint8_t* output, uint8_t key) {
    unsigned counter = 0;

    for (size_t i = 0; i < size; ++i) {
        output[i] = static_cast<uint8_t>((input[i] ^ key) - counter);

        ++counter;
        if (counter == 0x7f) {
            counter = 0;
        }
    }
}
```

For the known APK the key passed by `getFileData()` is `1`.

The paired encoder performs the inverse operation:

```cpp
output[i] = static_cast<uint8_t>((input[i] + counter) ^ key);
```

with the same `0..126` repeating counter.

## Validation against the original APK

`tools/asset_decoder.py` was run in dry-run validation mode against the known original APK.

Results:

| Type | Files | Decoded validation |
| --- | ---: | --- |
| `.lua` | 107 | 107 text scripts |
| `.png` | 97 | 97 valid PNG signatures |
| `.hpc` | 19 | 19 text/JSON-like documents |
| `.csv` | 14 | 14 text documents |
| **Total** | **237** | **0 validation failures** |

The recovered `StartLua.lua` is normal Lua source after decoding, confirming that the script critical path no longer depends on reverse engineering a custom VM or bytecode format.

Of the 107 Lua files, 99 decode as UTF-8 directly. Eight contain legacy Simplified-Chinese text in comments and validate as GB18030-compatible text; their Lua code remains ordinary source.

## Tool usage

Dry-run validation without writing proprietary decoded assets:

```bash
python3 tools/asset_decoder.py /path/to/com.hippiegame.nevergone.apk
```

Expected counts for the known original APK:

```text
processed: 237
.csv: 14
.hpc: 19
.lua: 107
.png: 97
validation failures: 0
```

Write decoded assets locally when needed for research/runtime testing:

```bash
python3 tools/asset_decoder.py /path/to/com.hippiegame.nevergone.apk \
  --output build/decoded-assets
```

Generate a metadata-only report:

```bash
python3 tools/asset_decoder.py /path/to/com.hippiegame.nevergone.apk \
  --json build/asset-decoding.json
```

APK mode intentionally processes only entries under `assets/`. Android resources under `res/` do not pass through this native game-file decoder and must not be transformed.

## Repository policy

The decoder is original project tooling and is safe to commit. Its locally produced files are still the game's copyrighted assets/scripts and must **not** be committed to this repository.

The recompilation should eventually support a user-supplied asset import step that performs this decoding locally during setup/build or first-run preparation.

## Consequences for the recompilation plan

This discovery substantially reduces the resource-pipeline risk:

- Lua can be treated as source-level game logic after local decoding.
- `.hpc` can be investigated as decoded text/config data instead of an opaque binary format.
- CSV tables are directly recoverable.
- PNG assets require the same transform before standard image parsing.
- The modern runtime can either reproduce the legacy decode-on-read behavior or decode once during an import/preparation step.

For maintainability, the preferred long-term design is likely a local import step: decode user-owned original assets into a clean runtime tree, validate them, then run the recompiled game against ordinary files rather than retaining the obfuscation in every file read.

## Next work

1. Build a Lua module/dependency index from decoded files without committing their contents.
2. Identify which Lua APIs are supplied by native C++ bindings.
3. Map `StartLua` initialization calls back to native singleton/binding implementations.
4. Determine the `.hpc` schemas and which native/Lua systems consume each configuration.
5. Trace first-run asset extraction into the writable `assets` directory documented in `resource-search-paths.md`.
