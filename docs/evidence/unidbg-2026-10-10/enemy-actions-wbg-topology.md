# EnemyActionsData WBG section topology

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records only structural facts recovered from the non-null `HPData` path of `EnemyActionsData::loadWBGFile(cocos2d::CCString*)` (`0x28f430`). Field names remain structural unless a downstream native use is already proven. In particular, the first 32-bit header word is called `header_word0`; the `0x68` comparison does not by itself justify naming it a format version.

## Fixed header

The parser begins at serialized offset zero with four 4-byte reads:

| Serialized offset | Reader | Destination |
|---:|---|---|
| `+0x00` | `HPData::getBytes(int&, HPRange)` at `0x28f4a0` | stack-local `header_word0` |
| `+0x04` | float reader at `0x28f4b4` | `EnemyActionsData+0xd4` |
| `+0x08` | float reader at `0x28f4ca` | `EnemyActionsData+0xd0` |
| `+0x0c` | unsigned reader at `0x28f4e2` | `EnemyActionsData+0xc4` |

The fixed header is therefore 16 bytes. The unsigned value stored at `+0xc4` controls several later loops.

## Section A — primary variable records

The first loop repeats `this+0xc4` times and appends `ActionFrameData` to the array at `this+0x88`; the `addObject` call is at `0x28f820`.

For each record, the proven serialized prefix is:

- `int32` at record `+0x00`;
- twelve `float32` values at `+0x04..+0x30`;
- `bool8` at `+0x34`;
- unaligned `int32` at `+0x35`;
- unaligned `float32` at `+0x39`;
- first variable-segment `int32` length at `+0x3d`.

The first payload begins at `+0x42`. Three variable segments use the same framing: `[int32 length][one skipped byte][length bytes]`. Thus a record occupies exactly `0x4c + len0 + len1 + len2` serialized bytes. The skipped byte and string contents are not assigned semantic names here.

## Section B — secondary fixed records

After Section A, an `int32` count is read at `0x28f842`. Each record is exactly 12 bytes: `int32`, `float32`, `float32`. A new `ActionFrameData` is appended to `this+0x8c` at `0x28f8be`.

## Section C — file-counted array fanout

At `0x28f8dc` the parser reads an outer `int32` count. For each outer index `i`, it reads an unsigned inner count at `0x28f904`, parses that many variable records, and appends each resulting `ActionFrameData` to the array pointer at:

`this + 0x14 + 4*i`

The `addObject` call is at `0x28fc3e`. The complete record field semantics and a safe bound for the file-provided outer count are not yet claimed.

## Section D — `header_word0`-gated array fanout

At `0x28fc5c` the original code compares `header_word0` with `0x68`:

- `header_word0 <= 0x68`: process 6 groups;
- `header_word0 > 0x68`: process 20 groups.

Each group begins with an `int32` record count read at `0x28fc7c`. Group `i` appends records to:

`this + 0x34 + 4*i`

The corresponding `addObject` call is at `0x28ffe8`. Twenty groups span the initialized array slots through `this+0x80`. This is recorded as a gate/fanout fact only; `header_word0` is not yet named a version field.

## Section E — root fixed records

The next `int32` count is read at `0x290018`. Each record is exactly `0x35` bytes:

- `int32` at `+0x00`;
- twelve `float32` values at `+0x04..+0x30`;
- `bool8` at `+0x34`.

Constructed `ActionFrameData` objects are appended to `this+0x84` at `0x290222`.

## Section F — 24-byte tuples and the initialized 100-entry regions

The parser then repeats once per primary record (`this+0xc4`). Each tuple is exactly `0x18` bytes:

- four `int32` reads at tuple offsets `0x00`, `0x04`, `0x08`, `0x0c`;
- two `float32` reads at `0x10`, `0x14`.

The six read callsites are `0x29027a`, `0x29028e`, `0x2902ac`, `0x2902ce`, `0x2902f8`, and `0x290328`.

The float at tuple `+0x10` is stored to `this+0xd8+4*i`; the float at `+0x14` is stored to `this+0x268+4*i`. These are exactly the two 100-dword regions initialized by `initWithFile`, providing a direct structural link between initialization and WBG loading.

The loop can create `ActionComboValue` objects and append them to arrays at `this+0x94`, `this+0x9c`, and `this+0x98` (`addObject` callsites `0x290368`, `0x290384`, `0x2903b4`). The branch conditions that decide which derived values are emitted remain structural and are not assigned gameplay meanings.

## Section G — final per-primary int table

Finally, the parser reads one `int32` per primary record at `0x2903f2`. If the primary array `this+0x88` is nonempty, it obtains `objectAtIndex(i)` and stores:

`1.0f / float(read_int)`

at `ActionFrameData+0x5c`; the store is at `0x29041a`. Each final-table entry is exactly 4 serialized bytes.

## Current boundary

This topology is enough to build bounded cursor accounting and section-order validation without fabricating field names. The project still does not claim a complete WBG schema: detailed records in Sections C and D, string meanings, combo-branch semantics, malformed-input behavior, and real-file validation remain unresolved until genuine user-owned WBG data is available for the bounded probe path.
