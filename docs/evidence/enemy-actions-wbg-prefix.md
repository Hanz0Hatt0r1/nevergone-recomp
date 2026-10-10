# EnemyActionsData WBG prefix and early ActionFrameData blocks

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note narrows the first data-dependent boundaries inside `EnemyActionsData::loadWBGFile(CCString*)`. It contains no original WBG payload and assigns semantic names only where the native control flow makes them unambiguous.

## Prefix

After `HPData::createWithContentsOfFile()` succeeds, the original initializes an `HPRange` pair and consumes exactly 16 bytes before entering its first object loop:

| Serialized offset | Native call site | Reader | Destination/use |
| ---: | ---: | --- | --- |
| `0x00` | `0x28f4a0` | `getBytes(int&, HPRange)` | stack local; later compared with `0x68`, semantic name unresolved |
| `0x04` | `0x28f4b4` | `getBytes(float&, HPRange)` | `EnemyActionsData + 0xd4` |
| `0x08` | `0x28f4ca` | `getBytes(float&, HPRange)` | `EnemyActionsData + 0xd0` |
| `0x0c` | `0x28f4e2` | `getBytes(unsigned int&, HPRange)` | `EnemyActionsData + 0xc4` |

At `0x28f4ec..0x28f4f2` a zero-based loop index is compared directly with the value stored at `+0xc4`. Every successful iteration reaches `ActionFrameData::createAFD()` at `0x28f77c` and adds that object to the `CCArray*` stored at `EnemyActionsData + 0x88` at `0x28f81a..0x28f820`. The `+0xc4` value is therefore the action-frame record count for this first loop.

The clean-room `Prefix` keeps the first integer and both floats structurally named because their gameplay meaning is not yet proven.

## First ActionFrameData record stream

For each record, the first fixed portion is read in this exact serialized order relative to the record start:

| Relative offset | Native call site | Type |
| ---: | ---: | --- |
| `0` | `0x28f538` | `int32` |
| `4` | `0x28f54e` | `float` |
| `8` | `0x28f562` | `float` |
| `12` | `0x28f576` | `float` |
| `16` | `0x28f58a` | `float` |
| `20` | `0x28f5a6` | `float` |
| `24` | `0x28f5be` | `float` |
| `28` | `0x28f5d2` | `float` |
| `32` | `0x28f5e6` | `float` |
| `36` | `0x28f5fa` | `float` |
| `40` | `0x28f60e` | `float` |
| `44` | `0x28f622` | `float` |
| `48` | `0x28f636` | `float` |
| `52` | `0x28f650` | `bool` |
| `53` | `0x28f664` | `int32` |
| `57` | `0x28f678` | `float` |
| `61` | `0x28f68e` | `int32` used as first string byte length |

The first char copy starts at relative offset `66`: after the length field the stream position advances by five bytes, proving one additional skipped byte between the four-byte length and the payload. `getBytes(char*, HPRange)` runs at `0x28f6a6`.

After the first payload, the same shape repeats twice: signed lengths are read at `0x28f6ce` and `0x28f71a`, each followed by one skipped byte and char copies at `0x28f6f4` and `0x28f736`. Thus one complete first-loop record consumes `76 + len1 + len2 + len3` bytes.

## Temporary string-buffer bound

The three original destination buffers begin at `sp+0xa4`, `sp+0x1a4`, and `sp+0x2a4`. Each is followed by an explicit NUL write at `buffer[length]`; the adjacent starts are exactly `0x100` bytes apart. The clean-room parser caps each payload at `0xff` bytes so its terminating NUL stays inside the observed 256-byte original buffer. This is a reconstruction safety bound, not evidence that the original parser validated hostile lengths.

## Second compact ActionFrameData block

When the first loop completes, the current stream offset is preserved in `r5`. The next block is fully bounded:

1. `0x28f842`: read one signed `int32` count;
2. `0x28f84c..0x28f854`: signed `BGE` loop guard;
3. each positive iteration reads `int32` at `0x28f870`, `float` at `0x28f882`, and `float` at `0x28f896`;
4. `0x28f89c`: create one `ActionFrameData`;
5. store the three values at `ActionFrameData + 0x74`, `+0x14`, and `+0x18`;
6. append it to `EnemyActionsData + 0x8c` at `0x28f8b8..0x28f8be`.

Zero or negative counts execute no iterations and consume only the four-byte count. Positive counts consume `4 + count * 12` bytes.

## Section C nested counted block

Immediately afterward the original enters a two-level counted structure:

- `0x28f8dc`: read a signed `int32` outer/group count;
- `0x28f8e6..0x28f8ec`: signed `BGE` outer guard, so a nonpositive value consumes only the four-byte outer count;
- `0x28f904`: for each positive outer index, read an unsigned `uint32` inner record count;
- `0x28f91c..0x28f922`: unsigned `BHS` inner guard;
- `0x28fc3c..0x28fc3e`: append every constructed record to the `CCArray*` selected from `EnemyActionsData + 0x14 + 4*outer_index`.

The inner record stream is fully recovered and consumes `72 + len1 + len2 + len3` bytes:

| Relative offset | Native call site | Type |
| ---: | ---: | --- |
| `0` | `0x28f962` | `int32` |
| `4..48` | `0x28f978..0x28fa66` | 12 × `float` |
| `52` | `0x28fa84` | `bool` |
| `53` | `0x28fa9c` | `int32` |
| `57` | `0x28fab0` | first signed string length |

The first payload starts at relative offset `62`, proving the same `[int32 length][one skipped byte][payload]` framing. The first char copy is at `0x28faca`; the second and third signed lengths are read at `0x28faee` and `0x28fb38`, with char copies at `0x28fb12` and `0x28fb4e`.

After the three strings, `ActionFrameData::createAFD()` runs at `0x28fb5a`. The inspected path writes the serialized scalar values into several `ActionFrameData` offsets, including `+0x74`, `+0x14..+0x48`, `+0x40`, and `+0x50`, and stores selected Cocos strings at later offsets. The clean-room parser deliberately preserves serialized order instead of assigning gameplay names from those destinations.

The repository's independent `enemy_actions_wbg_topology_evidence` contract proves the same container topology: Section C uses the file-provided outer count, an unsigned inner count per group, and array pointer `EnemyActionsData + 0x14 + 4*i`.

### Reconstruction safety

The parser preserves the original signed outer-loop behavior and unsigned inner count. For positive values it pre-bounds allocations against the remaining serialized bytes before `reserve`: every outer group requires at least its four-byte inner count and every inner record requires at least 72 bytes before variable payloads. The shared `0xff` string-payload cap applies to all three nested temporary buffers. Parse failure is transactional and leaves the caller's previous output unchanged.

## Reconstructed implementation

`enemy_actions_wbg_prefix.{h,cpp}` now implements:

- the 16-byte prefix;
- one complete primary `ActionFrameData` stream record;
- the compact counted `int32 + float + float` block;
- the Section C signed-outer/unsigned-inner nested block and its complete variable-length inner record;
- one-byte string separators, negative signed-length rejection, and the evidence-derived `0xff` payload cap;
- exact nonpositive signed-count behavior and pre-bounded positive allocations;
- transactional outputs for truncation or malformed lengths.

The implementation uses the project-owned bounds-checked `hp_data::Reader` / `Cursor`; it does not call the original library and does not require `CCFileUtils`, `AppParameters`, or Cocos object construction.

The next format step begins after Section C at `0x28fc52`: the first header word is compared against `0x68`, selecting 6 or 20 count-prefixed groups. The independent topology contract identifies their destination array base as `EnemyActionsData + 0x34 + 4*i`; the per-record serialized shape must remain evidence-bounded when that Section D parser is added.
