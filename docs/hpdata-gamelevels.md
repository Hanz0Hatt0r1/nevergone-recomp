# HPData / GameLevels reconstruction evidence

This note records clean-room metadata evidence for the binary level-data path used by `ChooseHeroReadyUIScene` and other scene loaders. It does not contain original game data or decompiler output.

## Loader chain

`ChooseHeroReadyUIScene::initLevelMap()` requests `gamescene/gs_list/pvp_scene.glData` through `GameScene::LoadGameLevelsWithFile()` at `0x0034fec0` in the checked-in Ghidra index.

That wrapper creates a `GameLevels` object, calls `GameLevels::LoadGameLevels()` at `0x002d2f1c`, then obtains the start scene port node.

`GameLevels::LoadGameLevels()` opens the supplied path using `HPData::createWithContentsOfFile()` and invokes the four binary sections in this recovered order:

1. `GameLevels::LoadGL_Scene()` — `0x002d27b8`
2. `GameLevels::LoadGL_Actions()` — `0x002d198c`
3. `GameLevels::LoadGL_Global()` — `0x002d1c5c`
4. `GameLevels::LoadGL_PortNode()` — `0x002d2090`

The checked-in call graph shows no textual field-name parser in those functions. They repeatedly consume typed binary values and construct `GameSceneData`, `GameSceneLayerData`, `GameSceneLayerObjectData`, action records, global/enemy data and port-node records.

## HPData evidence

Recovered functions in the checked-in Ghidra index:

- constructor — `0x002d64ac`
- `create` — `0x002d64e8`
- `createWithData` — `0x002d650a`
- `createWithContentsOfFile` — `0x002d652c`
- `getBytes(char*, HPRange)` — `0x002d6568`
- `getBytes(int*, HPRange)` — `0x002d658a`
- `getBytes(unsigned int*, HPRange)` — `0x002d65ac`
- `getBytes(float*, HPRange)` — `0x002d65ce`
- `getBytes(bool*, HPRange)` — `0x002d65f0`
- `length()` — `0x002d6612`

The constructor allocates its own buffer and copies the source bytes. `createWithContentsOfFile()` obtains file data through Cocos file utilities, constructs `HPData` from that data, then releases the temporary source buffer.

Focused ARMv7 disassembly of the user's original 1.0.9 library resolves the previously unknown `HPRange` semantics. In that binary the same symbols are located `0x10000` lower than the normalized checked-in Ghidra addresses (for example `LoadGL_Scene` starts at `0x002c27b8` and the unsigned `getBytes` overload at `0x002c65ac`). Every typed `getBytes` overload receives the two 32-bit `HPRange` words by value, then:

1. loads the internal HPData buffer pointer;
2. adds the first range word to that buffer pointer;
3. uses the second range word as the copy length;
4. copies exactly that many bytes into the supplied destination.

Therefore the original 32-bit `HPRange` value is conclusively `{byte_offset, byte_length}`. The clean-room reader still uses explicit `std::size_t` offsets/lengths instead of reproducing the old ABI structure internally.

## Current reader foundation

`hp_data_reader.{h,cpp}` provides an independent, bounds-checked byte-buffer implementation. It exposes raw byte copying plus explicit little-endian 32-bit integer/float and one-byte boolean helpers, matching the primitive types observed throughout `LoadGL_*` on the original little-endian ARM Android target.

It also exposes a bounded fixed-width string-field reader. That helper accepts an explicit offset and field width, rejects out-of-range slices, and stops the returned string at the first NUL byte. This matches the observed pattern where the original copies a fixed number of bytes, appends a NUL, then constructs a Cocos string.

A project-owned `hp_data::Cursor` layers sequential parsing on top of those explicit reader primitives. The cursor tracks only a current byte offset, advances after successful reads, and leaves its position unchanged on failed reads, seeks or skips. The reader/cursor are compiled into the Android native module and have host regression coverage for valid primitive/string reads, sequential advancement, seeking/skipping, and out-of-range rejection.

## Verified LoadGL_Scene top-level prefix

The ordered metadata trace plus ARMv7 control flow establish the beginning of `GameLevels::LoadGL_Scene()`:

1. a signed 32-bit value is read and stored in a `GameLevels` member whose semantic name is not yet recovered;
2. an unsigned 32-bit value is read;
3. a zero-based loop index is compared directly against that unsigned value before `GameSceneData::create()`.

The second value is therefore the scene count. No scene-local bytes are consumed when it is zero.

`game_levels_scene_prefix::parse()` models only this top-level prefix as `{first_i32, scene_count}` and consumes exactly 8 bytes. The unresolved first field remains deliberately opaque.

## Verified first-scene header

When `scene_count > 0`, the original first scene iteration performs this verified sequence:

1. read one `uint32` at Ghidra call site `0x002d284a`;
2. advance the external stream offset by **5** bytes from the start of that uint32, meaning the four length bytes plus one additional byte whose purpose is still unknown;
3. use the just-read `uint32` as `HPRange.byte_length` for `getBytes(char*, ...)` at `0x002d2868`;
4. advance by that same payload length and append a NUL at `buffer[length]`;
5. read two 4-byte floats at `0x002d2892` and `0x002d28a8` and assign them as a `CCPoint`;
6. build a `CCString` from the copied char payload;
7. read another `uint32` at `0x002d28f2`;
8. initialize a zero-based loop index and compare it directly against that value before each `GameSceneLayerData::create()` call.

The third value is therefore the first scene's string byte length, and the later uint32 is the scene's layer count. The intervening single byte is confirmed as skipped but is intentionally left semantically unnamed.

`game_levels_scene_prefix::parse_first_scene_header()` implements exactly that evidence. It returns the top-level prefix, string length/string, two point floats, layer count, and verified byte count. Parsing is transactional. A zero scene count, truncated string, missing float/layer-count field, or impossible string length causes failure without exposing a partially updated output. For a string length `N`, the verified first-scene header ends at byte offset `25 + N`.

## Verified first-layer header

When the first scene's `layer_count > 0`, the first layer begins immediately after the scene header. The original ordered call/control flow shows:

1. `HPData::getBytes(float*, ...)` at `0x002d293e`;
2. the stream offset advances by 4 bytes;
3. `HPData::getBytes(unsigned int*, ...)` at `0x002d2960`;
4. a zero-based loop index is compared directly against that uint32 before `GameSceneLayerObjectData::create()` at `0x002d297e`.

The uint32 is therefore the layer's object count. The float's semantic purpose is not yet proven and remains deliberately named `first_float`.

`game_levels_scene_prefix::parse_first_layer_header()` extends the first-scene header by exactly 8 verified bytes and returns `{first_float, object_count}` plus the total consumed byte count. It fails cleanly when `layer_count == 0`, when either primitive is truncated, or when an earlier scene-header boundary is invalid.

## Verified first-object prefix

A dedicated ordered-call metadata probe of the object body establishes the first reads after `GameSceneLayerObjectData::create()`:

1. `HPData::getBytes(int*, ...)` at `0x002d29a4`;
2. `HPData::getBytes(unsigned int*, ...)` at `0x002d29bc`;
3. the next operation is an unresolved-width `HPData::getBytes(char*, ...)` at `0x002d29e6`.

The first two fields form an immediately sequential 8-byte prefix. Their semantic meanings are not yet proven, so `parse_first_object_prefix()` exposes them only as `first_i32` and `second_u32`. It requires `object_count > 0`, updates output only when both values are present, and stops before the char field.

## Imported GameLevels asset probe

The reconstructed runtime resolves the user-owned app-private resource:

`<files>/assets/gamescene/gs_list/pvp_scene.glData`

The probe checks that the path exists and is a regular file, obtains its size, enforces a 64 MiB upper bound, then loads the bytes into the reconstructed `hp_data::Reader`. It validates the 8-byte top-level prefix, the first-scene header when a scene exists, the first-layer header when a layer exists, and the first-object prefix when an object exists.

Bootstrap diagnostics report only readiness and verified byte counts. They do **not** print the imported scene's strings, coordinates, counts, floats, object fields, or other proprietary field values. Synthetic host regressions cover missing/readable/oversized/non-regular paths, zero scenes, zero layers, zero objects, valid nested headers, truncated fields, and hostile string lengths without requiring game data.

## Remaining format work

Recover the exact range construction for the first object `char*` read at `0x002d29e6`. Only after its byte width/offset is proven should reconstruction advance into the five following floats and the remaining object fields. Parsing a second object/layer before the complete object-record size/schema is known would be speculative.

Semantic names should be assigned only when the value's use in the original code makes them unambiguous.
