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

After the first payload, the same shape repeats twice:

1. read one signed `int32` length (`0x28f6ce`), advance five bytes from the length-field start, then copy that many chars at `0x28f6f4`;
2. read another signed `int32` length (`0x28f71a`), advance five bytes, then copy that many chars at `0x28f736`.

Thus one complete first-loop record consumes `76 + len1 + len2 + len3` bytes. All three lengths are read through the signed-int overload and then reused as byte counts. The reconstructed parser rejects negative lengths.

## Temporary string-buffer bound

The three original destination buffers begin at `sp+0xa4`, `sp+0x1a4`, and `sp+0x2a4`. Each is followed by an explicit NUL write at `buffer[length]`; the adjacent starts are exactly `0x100` bytes apart. The clean-room parser therefore caps each payload at `0xff` bytes so its terminating NUL would remain inside the corresponding observed 256-byte original buffer. This is a safety bound in the reconstruction, not evidence that the original file parser validated hostile lengths.

## First object-construction boundary

After all three strings have been consumed, the original creates an `ActionFrameData`, copies the scalar locals into it, creates/retains Cocos strings for selected payloads, and appends the object to the array at `EnemyActionsData + 0x88`. Some serialized locals are reordered before storage and one string buffer is not retained on the inspected path. The clean-room stream parser intentionally preserves serialized order rather than inventing field names from destination offsets.

## Second compact ActionFrameData block

When the first loop completes, the current stream offset is preserved in `r5`. The next native block is fully bounded:

1. `0x28f842`: read one signed `int32` count;
2. initialize a zero-based signed loop index;
3. `0x28f84c..0x28f854`: compare the index against that signed count with `BGE`;
4. for each positive iteration read exactly:
   - `int32` at `0x28f870`;
   - `float` at `0x28f882`;
   - `float` at `0x28f896`;
5. `0x28f89c`: create one `ActionFrameData`;
6. store those three values at `ActionFrameData + 0x74`, `+0x14`, and `+0x18` respectively;
7. `0x28f8b8..0x28f8be`: append the object to the `CCArray*` at `EnemyActionsData + 0x8c`;
8. advance by exactly 12 serialized bytes per iteration and repeat.

Because the branch is signed, zero or negative counts execute no iterations and consume only the four-byte count. Positive counts consume `4 + count * 12` bytes. The reconstructed parser pre-bounds a positive count against the remaining byte length before reserving its vector, preventing hostile allocation sizes while preserving the original signed-loop behavior for nonpositive counts.

Immediately after this compact block, the original advances to the next signed count read at `0x28f8dc`. That begins a different, nested record shape and is deliberately outside the current parser boundary.

## Reconstructed implementation

`enemy_actions_wbg_prefix.{h,cpp}` implements:

- the 16-byte prefix;
- one complete first-loop `ActionFrameData` stream record from an explicit start offset;
- the three one-byte string separators;
- signed-length rejection and the evidence-derived `0xff` payload cap;
- the following compact counted `ActionFrameData` block (`int32 + float + float` per positive iteration);
- exact signed nonpositive-count behavior for that compact block;
- transactional outputs: truncation or malformed lengths leave caller output unchanged.

The implementation uses the existing project-owned bounds-checked `hp_data::Reader` / `Cursor`; it does not call the original library and does not require `CCFileUtils`, `AppParameters`, or Cocos object construction.

The next format step is the nested counted block beginning with the signed count at `0x28f8dc` and the per-outer-record unsigned count at `0x28f904`. Its inner record shape must be recovered completely before it is added to the clean-room parser.
