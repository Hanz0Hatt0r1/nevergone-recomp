# EnemyActionsData WBG prefix and initial sections

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note narrows the first data-dependent boundary inside `EnemyActionsData::loadWBGFile(CCString*)`. It contains no original WBG payload and assigns semantic names only where the native control flow makes them unambiguous.

## Prefix

After `HPData::createWithContentsOfFile()` succeeds, the original consumes exactly 16 bytes before entering its first object loop:

| Serialized offset | Native call site | Reader | Destination/use |
| ---: | ---: | --- | --- |
| `0x00` | `0x28f4a0` | `getBytes(int&, HPRange)` | stack local; later compared with `0x68`, semantic name unresolved |
| `0x04` | `0x28f4b4` | `getBytes(float&, HPRange)` | `EnemyActionsData + 0xd4` |
| `0x08` | `0x28f4ca` | `getBytes(float&, HPRange)` | `EnemyActionsData + 0xd0` |
| `0x0c` | `0x28f4e2` | `getBytes(unsigned int&, HPRange)` | `EnemyActionsData + 0xc4` |

At `0x28f4ec..0x28f4f2` a zero-based loop index is compared directly with the value stored at `+0xc4`. Every successful iteration reaches `ActionFrameData::createAFD()` at `0x28f77c` and adds that object to the `CCArray*` stored at `EnemyActionsData + 0x88` at `0x28f81a..0x28f820`. The `+0xc4` value is therefore the action-frame record count for this first loop.

The clean-room `Prefix` keeps the first integer and both floats structurally named because their gameplay meaning is not yet proven.

## Section A — ActionFrameData record stream

For each record, the first fixed portion is read in this exact serialized order relative to the record start:

| Relative offset | Native call site | Type |
| ---: | ---: | --- |
| `0` | `0x28f538` | `int32` |
| `4..48` | `0x28f54e..0x28f636` | twelve `float32` values |
| `52` | `0x28f650` | `bool8` |
| `53` | `0x28f664` | `int32` |
| `57` | `0x28f678` | `float32` |
| `61` | `0x28f68e` | `int32` used as first payload byte length |

The first char copy starts at relative offset `66`: after the length field the stream position advances by five bytes, proving one additional skipped byte between the four-byte length and the payload. `getBytes(char*, HPRange)` runs at `0x28f6a6`.

After the first payload, the same shape repeats twice:

1. read one signed `int32` length (`0x28f6ce`), advance five bytes from the length-field start, then copy that many chars at `0x28f6f4`;
2. read another signed `int32` length (`0x28f71a`), advance five bytes, then copy that many chars at `0x28f736`.

Thus one complete Section A record consumes `76 + len0 + len1 + len2` bytes. All three lengths are read through the signed-int overload and then reused as byte counts. The reconstructed parser rejects negative lengths.

The reconstructed `parse_initial_sections()` now iterates exactly `Prefix::action_frame_count` records. Before entering the loop it requires that the remaining input could contain at least `action_frame_count * 76` bytes. This is a lower-bound safety check only; each actual record still performs all payload and truncation checks transactionally.

## Temporary string-buffer bound

The three original destination buffers begin at `sp+0xa4`, `sp+0x1a4`, and `sp+0x2a4`. Each is followed by an explicit NUL write at `buffer[length]`; the adjacent starts are exactly `0x100` bytes apart. The clean-room parser therefore caps each payload at `0xff` bytes so its terminating NUL would remain inside the corresponding observed 256-byte original buffer. This is a safety bound in the reconstruction, not evidence that the original file parser validated hostile lengths.

## Section B — counted 12-byte records

Immediately after all Section A records, the original reads one signed `int32` count at `0x28f842`. The loop initializes its index to zero, loads the signed count, and uses `bge` after comparing `index` with `count` at `0x28f84c..0x28f854`. Therefore values `<= 0` execute zero Section B iterations.

Every positive-count iteration consumes exactly 12 bytes:

| Relative offset | Native call site | Type |
| ---: | ---: | --- |
| `0` | `0x28f870` | `int32` |
| `4` | `0x28f882` | `float32` |
| `8` | `0x28f896` | `float32` |

After the 12 bytes, the original creates an `ActionFrameData` at `0x28f89c`, stores the three values into that object, and appends it to `EnemyActionsData+0x8c` through `CCArray::addObject()` at `0x28f8be`.

The clean-room representation keeps the Section B count as signed `secondary_record_count_i32` and mirrors the proven zero-iteration behavior for `count <= 0`. For a positive count, it first checks that the remaining input could contain at least `count * 12` bytes, preventing a file-controlled count from causing an unbounded record loop before byte availability is established.

## Transactional reconstructed boundary

`enemy_actions_wbg_prefix.{h,cpp}` now provides:

- `parse_prefix()` for the fixed 16-byte prefix;
- `parse_action_frame_record()` for one complete Section A record;
- `parse_secondary_record()` for one 12-byte Section B record;
- `parse_initial_sections()` for the complete prefix + all Section A records + Section B count + all Section B records.

`InitialSections::bytes_consumed` points to the first byte after Section B. Every public parser is transactional: truncation, an invalid Section A payload length, or an impossible positive count leaves caller output unchanged. The implementation uses the project-owned bounds-checked `hp_data::Reader` / `Cursor`; it does not call the original library and does not require `CCFileUtils`, `AppParameters`, or Cocos object construction.

## Current boundary

The next native read after Section B begins the file-counted fanout at `0x28f8dc`. Structural evidence for the later WBG sections is tracked separately in `docs/evidence/unidbg-2026-10-10/enemy-actions-wbg-topology.md`. The next parser milestone is to consume that Section C outer count and its counted records without assigning gameplay names to unresolved fields.
