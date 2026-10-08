# HPData / GameLevels reconstruction evidence

This note records clean-room metadata evidence for the binary level-data path used by `ChooseHeroReadyUIScene` and other scene loaders. It does not contain original game data or decompiler output.

## Loader chain

`ChooseHeroReadyUIScene::initLevelMap()` requests `gamescene/gs_list/pvp_scene.glData` through `GameScene::LoadGameLevelsWithFile()` at `0x0034fec0`.

That wrapper creates a `GameLevels` object, calls `GameLevels::LoadGameLevels()` at `0x002d2f1c`, then obtains the start scene port node.

`GameLevels::LoadGameLevels()` opens the supplied path using `HPData::createWithContentsOfFile()` and invokes the four binary sections in this recovered order:

1. `GameLevels::LoadGL_Scene()` — `0x002d27b8`
2. `GameLevels::LoadGL_Actions()` — `0x002d198c`
3. `GameLevels::LoadGL_Global()` — `0x002d1c5c`
4. `GameLevels::LoadGL_PortNode()` — `0x002d2090`

The checked-in call graph shows no textual field-name parser in those functions. They repeatedly consume typed binary values and construct `GameSceneData`, `GameSceneLayerData`, `GameSceneLayerObjectData`, action records, global/enemy data and port-node records.

## HPData evidence

Recovered functions:

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

The constructor allocates its own buffer and copies the source bytes with `malloc` + `memcpy`. `createWithContentsOfFile()` obtains file data through Cocos file utilities, constructs `HPData` from that data, then releases the temporary source buffer. The typed `getBytes` overloads are tiny leaf functions with no resolved callees, consistent with direct buffer extraction rather than a text decoder or higher-level serialization library.

A focused checked-in metadata probe also confirms the read ordering inside the binary sections. `LoadGL_Scene()` starts with integer/unsigned reads, creates `GameSceneData`, then performs a `char*` read followed by float reads before the resulting char buffer reaches `CCString::create`. The same `char* -> CCString::create` pattern appears again in layer/object parsing. `LoadGL_PortNode()` similarly mixes unsigned, char and bool reads before creating `GameScenePortNodeData`. This establishes fixed-width/raw character fields as a required primitive without exposing or guessing the internal `HPRange` layout.

The full metadata index contains the five `HPData::getBytes(..., HPRange)` signatures and mangled imported labels, which confirms that `HPRange` is passed by value. It does not contain separately named `HPRange` constructors, operators or field symbols, so the structure's field layout and offset/length semantics remain unresolved and are not inferred from likely layouts.

## Current reader foundation

`hp_data_reader.{h,cpp}` provides an independent, bounds-checked byte-buffer foundation for future clean-room parsers. It deliberately accepts explicit offsets rather than claiming the unresolved in-memory `HPRange` field layout. It exposes raw byte copying plus explicit little-endian 32-bit integer/float and one-byte boolean helpers, matching the primitive types observed throughout `LoadGL_*` on the original little-endian ARM Android target.

It also exposes a bounded fixed-width string-field reader. That helper accepts an explicit offset and field width, rejects out-of-range slices, and stops the returned string at the first NUL byte. This maps only the verified raw-char-field behavior needed before `CCString::create`; it does not claim an encoding conversion or an `HPRange` ABI.

A project-owned `hp_data::Cursor` layers sequential parsing on top of those explicit reader primitives. The cursor tracks only a current byte offset, advances after successful reads, and leaves its position unchanged on failed reads, seeks or skips. This gives future `LoadGL_*` reconstruction a safe stream-style API without claiming that the cursor mirrors the original `HPRange` structure or calling convention.

The reader/cursor are compiled into the Android native module and have host regression coverage for valid primitive/string reads, sequential advancement, seeking/skipping, and out-of-range rejection.

## Verified LoadGL_Scene prefix

The checked-in ordered call trace now proves the first three stream reads in `GameLevels::LoadGL_Scene()`:

1. `HPData::getBytes(int*, ...)` at call site `0x002d27f0`;
2. `HPData::getBytes(unsigned int*, ...)` at `0x002d2806`;
3. `GameSceneData::create()` at `0x002d2824` (no HPData consumption);
4. `HPData::getBytes(unsigned int*, ...)` at `0x002d284a`.

The next stream read is the unresolved-width `char*` field at `0x002d2868`, so reconstruction intentionally stops before it.

`game_levels_scene_prefix.{h,cpp}` therefore parses exactly three opaque values: `first_i32`, `second_u32`, and `third_u32`. It consumes exactly 12 bytes through the project-owned sequential cursor and updates its output only when all three reads succeed. Truncated input fails without exposing a partially parsed prefix.

This is intentionally not presented as a recovered `GameSceneData` schema. It is a narrow executable contract for the starting fields whose type/order are currently proven; semantic names and the following character-field width remain unresolved.

## Imported GameLevels asset probe

The reconstructed runtime now has a narrow readiness probe for the first recovered scene resource. It resolves only this user-owned app-private path:

`<files>/assets/gamescene/gs_list/pvp_scene.glData`

The probe checks that the path exists and is a regular file, obtains its size, enforces a 64 MiB upper bound, then loads the bytes into the reconstructed `hp_data::Reader`. It also attempts the verified three-field `LoadGL_Scene` prefix and reports only whether those first 12 bytes are readable; bootstrap diagnostics do not print the proprietary field values themselves.

This remains a transport/readiness bridge plus a minimal verified parser rather than a full format implementation. It proves that the imported runtime resource can reach both the clean-room HPData reader and the recovered sequential read order while keeping `HPRange`, the following char-field width, later loop counts and object schemas as separate reverse-engineering tasks. Synthetic host regressions cover missing/readable/oversized/non-regular paths, successful 12-byte prefix parsing and truncated-prefix rejection without requiring any game data.

## Remaining format work

Recover the exact width/semantic identity of the `char*` field read at `0x002d2868`, then continue with the two following float reads (`0x002d2892`, `0x002d28a8`) and the first nested layer-count read at `0x002d28f2`. The unresolved original `HPRange` ABI should still be recovered from focused Ghidra evidence when needed for binary-compatibility analysis, but the clean-room sequential parser does not need to imitate that ABI internally.
