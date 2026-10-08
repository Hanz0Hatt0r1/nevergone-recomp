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

The reader is compiled into the Android native module and has a host regression for valid primitive/string reads and out-of-range rejection.

## Imported GameLevels asset probe

The reconstructed runtime now has a narrow readiness probe for the first recovered scene resource. It resolves only this user-owned app-private path:

`<files>/assets/gamescene/gs_list/pvp_scene.glData`

The probe checks that the path exists and is a regular file, obtains its size, enforces a 64 MiB upper bound, then loads the bytes into the reconstructed `hp_data::Reader`. Bootstrap diagnostics expose only availability/read status and byte count. They do not dump, decode, persist, hash or otherwise report the proprietary contents.

This is deliberately a transport/readiness bridge rather than a format parser. It proves that the imported runtime resource can reach the clean-room HPData reader while keeping `HPRange`, field order, loop counts and object schemas as separate reverse-engineering tasks. A synthetic host regression covers missing, readable, oversized and non-regular paths without requiring any game data.

## Remaining format work

Before wiring this reader to a reconstructed `GameLevels` schema, recover the exact `HPRange` offset/length semantics and then map field order/count loops in each `LoadGL_*` function. Numeric/object layout evidence should come from the focused Ghidra exporter or equivalent metadata; field order and loop counts must not be guessed from likely game structures.
