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

Recovered functions in the checked-in Ghidra index include `createWithContentsOfFile` at `0x002d652c` and typed `getBytes` overloads at `0x002d6568..0x002d65f0`.

Focused ARMv7 disassembly of the user's original 1.0.9 library resolves the original 32-bit `HPRange` value as `{byte_offset, byte_length}`. In that binary the same symbols are located `0x10000` lower than the normalized checked-in Ghidra addresses. Each typed `getBytes` overload adds the first range word to the internal buffer pointer and copies exactly the second range word's number of bytes.

The clean-room reader uses explicit `std::size_t` offsets/lengths rather than reproducing the old ABI structure internally.

## Current reader foundation

`hp_data_reader.{h,cpp}` provides a bounds-checked byte buffer with little-endian integer/float/boolean reads, bounded fixed-width strings, and a sequential `hp_data::Cursor`. Failed reads do not advance the cursor.

## Verified LoadGL_Scene top-level prefix

The ordered metadata trace plus ARMv7 control flow establish:

1. one unresolved signed 32-bit value;
2. one unsigned 32-bit value used directly as the scene-loop bound.

`game_levels_scene_prefix::parse()` models this as `{first_i32, scene_count}` and consumes exactly 8 bytes.

## Verified first-scene header

When `scene_count > 0`, the first scene reads:

1. a `uint32` string byte length at `0x002d284a`;
2. one additional skipped byte of unresolved meaning;
3. exactly that many char bytes at `0x002d2868`, later NUL-terminated;
4. two floats at `0x002d2892` / `0x002d28a8`, assigned as a `CCPoint`;
5. a `uint32` at `0x002d28f2` used directly as the layer-loop bound.

`parse_first_scene_header()` returns the string, point and proven `layer_count` transactionally. For a string length `N`, this boundary ends at byte offset `25 + N`.

## Verified first-layer header

When `layer_count > 0`, the first layer begins with:

1. an unresolved float at `0x002d293e`;
2. a `uint32` at `0x002d2960` used directly as the object-loop bound before `GameSceneLayerObjectData::create()` at `0x002d297e`.

`parse_first_layer_header()` therefore adds exactly 8 verified bytes and returns `{first_float, object_count}` without assigning a guessed meaning to the float.

## Verified first-object prefix

A dedicated ordered-call metadata probe of the object body establishes the first reads after `GameSceneLayerObjectData::create()`:

1. `HPData::getBytes(int*, ...)` at `0x002d29a4`;
2. `HPData::getBytes(unsigned int*, ...)` at `0x002d29bc`;
3. the next operation is an unresolved-width `HPData::getBytes(char*, ...)` at `0x002d29e6`.

The first two fields are therefore an immediately sequential 8-byte prefix. Their semantic meanings are not yet proven, so `parse_first_object_prefix()` exposes them only as `first_i32` and `second_u32`. It requires `object_count > 0`, updates output only when both values are present, and deliberately stops before the char field.

The remainder of the object record contains five early float reads, another int, two bools, and additional mixed fields before `GameSceneLayerData::AddObject()` at `0x002d2dc2`. Those later fields are not yet parsed because the first char field's exact width/offset behavior must be established first.

## Imported GameLevels asset probe

The reconstructed runtime resolves the user-owned app-private resource:

`<files>/assets/gamescene/gs_list/pvp_scene.glData`

The readiness probe loads it through the clean-room `hp_data::Reader` and now validates the top-level prefix, first scene, first layer, and first object prefix where those nested records exist.

Bootstrap diagnostics report only readiness and verified byte counts. They do **not** print imported strings, coordinates, counts, floats, object fields, or other proprietary values. Synthetic host regressions cover missing/truncated records, zero-count nesting and hostile upstream string lengths without requiring game data.

## Remaining format work

Recover the exact range construction for the first object `char*` read at `0x002d29e6`. Only after its byte width/offset is proven should reconstruction advance into the five following floats and the remaining object fields. Parsing a second object/layer before the complete object-record size is known would be speculative.
