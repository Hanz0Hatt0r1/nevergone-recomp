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

1. a signed 32-bit value is read directly into `GameLevels + 0x3c` at Ghidra `0x002d27f0`;
2. an unsigned 32-bit value is read;
3. a zero-based loop index is compared directly against that unsigned value before `GameSceneData::create()`.

The second value is therefore the scene count. No scene-local bytes are consumed when it is zero. The first signed field remains semantically unnamed, but later object parsing proves that it acts as a format gate.

`game_levels_scene_prefix::parse()` models this top-level prefix as `{first_i32, scene_count}` and consumes exactly 8 bytes.

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

The first uint32 is therefore the scene string byte length, and the later uint32 is the scene layer count. The intervening single byte is confirmed as skipped but intentionally left semantically unnamed.

`parse_first_scene_header()` implements exactly that evidence. It is transactional and, for a scene string length `N`, ends at byte offset `25 + N`.

## Verified first-layer header

When the first scene's `layer_count > 0`, the first layer begins immediately after the scene header. The original ordered call/control flow shows:

1. `HPData::getBytes(float*, ...)` at `0x002d293e`;
2. the stream offset advances by 4 bytes;
3. `HPData::getBytes(unsigned int*, ...)` at `0x002d2960`;
4. a zero-based loop index is compared directly against that uint32 before `GameSceneLayerObjectData::create()` at `0x002d297e`.

The uint32 is therefore the layer object count. The float's semantic purpose is not yet proven and remains deliberately named `first_float`.

`parse_first_layer_header()` extends the first-scene header by exactly 8 verified bytes and returns `{first_float, object_count}` plus the total consumed byte count.

## Verified first-object core

Focused Thumb/ARMv7 disassembly resolves the first object string and the fixed fields that follow it.

The object begins with:

1. `HPData::getBytes(int*, ...)` at Ghidra `0x002d29a4`;
2. `HPData::getBytes(unsigned int*, ...)` at `0x002d29bc`;
3. the second value is written directly to `HPRange.byte_length` before the char copy;
4. the stream offset advances by **5** bytes from the start of that uint32, so the four length bytes plus one skipped byte precede the payload;
5. `HPData::getBytes(char*, ...)` runs at `0x002d29e6` with that exact byte length;
6. after the copy, the stream offset advances by the same length and the original writes a NUL at `buffer[length]`.

The historical `FirstObjectPrefix::second_u32` field is therefore conclusively the object string byte length. The source-compatible name is retained for now.

Immediately after the string, the original performs these typed reads in order:

1. five `float` reads at `0x002d2a14`, `0x002d2a2a`, `0x002d2a40`, `0x002d2a62`, and `0x002d2a78`;
2. one `int32` read at `0x002d2a8e`;
3. two one-byte `bool` reads at `0x002d2aae` and `0x002d2ac6`.

The stores prove structural grouping without speculative gameplay names: floats 1–2 are assigned as one `CCPoint`, float 3 is a scalar, floats 4–5 form a second `CCPoint`, then the int32 and two bools are stored consecutively.

`parse_first_object_core()` implements exactly this fixed boundary and stops immediately before the first version-gated block. For object string length `N`, this object portion consumes `35 + N` bytes from the beginning of the object record.

## Verified version-gated uint32 vector

The next branch is now recovered directly from ARMv7 control flow:

1. at Ghidra `0x002d2b20`, `LoadGL_Scene` reloads `GameLevels + 0x3c`, the same signed value read as the top-level `Prefix::first_i32`;
2. it compares that value with signed constant `2` and skips the entire block when `first_i32 <= 2`;
3. for `first_i32 > 2`, it reads one `uint32` count at `0x002d2b48`;
4. it initializes a zero-based index and loops until the index reaches that count;
5. each iteration reads one `uint32` at `0x002d2b72` and appends it to the object-owned `std::vector<uint32_t>` at object offset `+0x5c`.

`parse_first_object_version_extension()` reproduces only this verified gate. For top-level `first_i32 <= 2`, it succeeds without consuming bytes past `FirstObjectCore`. For values `> 2`, it reads the count and exactly `count` uint32 values with a pre-allocation bounds check that rejects hostile counts before reserving memory.

The parser stops at Ghidra `0x002d2ba4`, before the next independent branch on `FirstObjectPrefix::first_i32`. No later conditional object fields are guessed.

## Imported GameLevels asset probe

The reconstructed runtime resolves the user-owned app-private resource:

`<files>/assets/gamescene/gs_list/pvp_scene.glData`

The probe checks that the path exists and is a regular file, obtains its size, enforces a 64 MiB upper bound, then loads the bytes into the reconstructed `hp_data::Reader`. It validates each evidence-backed boundary through the first object's version-gated uint32 vector.

Bootstrap diagnostics report only readiness and verified byte counts. They do **not** print imported scene strings, coordinates, counts, floats, object fields, or vector values. Synthetic host regressions cover truncated fields, hostile string lengths, the signed version gate, zero/positive vector counts, and hostile vector counts without requiring game data.

The reconstructed `cpp_OnEnterGame` route treats `first-object-version-extension-verified` as the strongest current GameLevels entry state. It still does not instantiate or render a gameplay scene.

## Remaining format work

After the optional uint32 vector, the original checks the object's leading `first_i32`. A zero value jumps directly to adding the object to its layer. A nonzero value enters another conditional block whose shape depends again on the top-level format gate and then on additional object values.

Recover that branch through its join point before attempting to parse a second object or layer. In particular, the complete object-record size is not fixed yet for nonzero object types, so iterating object records before those cases are proven would be speculative.

Semantic names should be assigned only when the value's use in the original code makes them unambiguous.
