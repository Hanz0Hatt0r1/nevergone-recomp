# EnemyActionsData WBG prefix and early ActionFrameData blocks

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note narrows the first data-dependent boundaries inside `EnemyActionsData::loadWBGFile(CCString*)`. It contains no original WBG payload and assigns semantic names only where the native control flow makes them unambiguous.

## Prefix

After `HPData::createWithContentsOfFile()` succeeds, the original consumes exactly 16 bytes before entering its first object loop:

| Serialized offset | Native call site | Reader | Destination/use |
| ---: | ---: | --- | --- |
| `0x00` | `0x28f4a0` | `getBytes(int&, HPRange)` | stack local; later compared with `0x68`, semantic name unresolved |
| `0x04` | `0x28f4b4` | `getBytes(float&, HPRange)` | `EnemyActionsData + 0xd4` |
| `0x08` | `0x28f4ca` | `getBytes(float&, HPRange)` | `EnemyActionsData + 0xd0` |
| `0x0c` | `0x28f4e2` | `getBytes(unsigned int&, HPRange)` | `EnemyActionsData + 0xc4` |

At `0x28f4ec..0x28f4f2` a zero-based loop index is compared directly with `+0xc4`. Every successful iteration creates an `ActionFrameData` and appends it to the array at `EnemyActionsData + 0x88`. The `+0xc4` value is therefore the first action-frame record count.

## Primary ActionFrameData record

Each primary record begins with `int32`, twelve `float32`, `bool8`, `int32`, `float32`, then three variable segments framed as `[int32 length][one skipped byte][length bytes]`. The fixed size excluding payload bytes is 76, so total record size is `76 + len1 + len2 + len3`. The three original temporary char buffers are `0x100` bytes apart and receive explicit trailing NUL writes, so the clean-room parser caps each payload at `0xff` bytes.

## Section B: compact ActionFrameData block

After the primary loop, `0x28f842` reads a signed `int32` count. Each positive iteration consumes `int32 + float32 + float32` (12 bytes) and appends an `ActionFrameData` to `EnemyActionsData + 0x8c`. Nonpositive counts consume only the four-byte count.

## Section C: nested counted groups

Section C reads a signed outer count at `0x28f8dc`; each positive outer group then reads an unsigned inner count at `0x28f904`. Records are appended through the array pointer at `EnemyActionsData + 0x14 + 4*i`. Each inner record consumes `72 + len1 + len2 + len3` bytes: `int32 + 12×float32 + bool8 + int32 + 3 framed strings`.

## Section D: version-gated groups

At `0x28fc5c`, header word 0 is compared with `0x68`: values `> 0x68` select 20 groups, otherwise 6. Every selected group has a signed `int32` record count and targets `EnemyActionsData + 0x34 + 4*i`. Each record consumes `80 + len1 + len2 + len3` bytes: `int32 + 12×float32 + bool8 + int32 + 2×uint32 + 3 framed strings`. All 6 or 20 group headers are mandatory; positive counts are bounded after reserving the remaining mandatory headers.

## Section E: fixed tail

`0x290018` reads a signed `int32` count. Each positive iteration consumes exactly `0x35` bytes:

- `int32` at relative `+0`;
- twelve `float32` at `+4..+48`;
- `bool8` at `+52`.

`ActionFrameData::createAFD()` runs at `0x29018c`, and the object is appended to `EnemyActionsData + 0x84` at `0x29021c..0x290222`. Nonpositive counts consume only the four-byte count. Positive counts are bounded against `remaining / 0x35` before allocation.

## Section F: primary-indexed 24-byte tuples

After Section E, the parser does **not** read another count. Instead it repeats exactly the unsigned primary count already stored at `EnemyActionsData + 0xc4`. For each primary index `i`, it consumes one fixed `0x18`-byte tuple:

| Relative offset | Native call site | Type |
| ---: | ---: | --- |
| `0x00` | `0x29027a` | `int32` |
| `0x04` | `0x29028e` | `int32` |
| `0x08` | `0x2902ac` | `int32` |
| `0x0c` | `0x2902ce` | `int32` |
| `0x10` | `0x2902f8` | `float32` |
| `0x14` | `0x290328` | `float32` |

The float at tuple `+0x10` is stored to `EnemyActionsData + 0xd8 + 4*i`; the float at `+0x14` is stored to `EnemyActionsData + 0x268 + 4*i`. Those are exactly the two 100-dword regions initialized by `initWithFile`, providing an independent structural cross-check of the tuple count and indexing.

The four integer values feed branch logic that can create `ActionComboValue` objects and append them to arrays at `EnemyActionsData + 0x94`, `+0x9c`, and `+0x98` (`addObject` at `0x290368`, `0x290384`, `0x2903b4`). Their gameplay meanings are unresolved, so the clean-room stream record keeps them as an ordered four-element integer array rather than naming them from branch behavior.

`parse_primary_indexed_tuple_block()` receives `action_frame_count` from the already parsed prefix instead of consuming a new serialized count. It pre-bounds `count * 24` via `count <= remaining / 24`, parses exactly that many records from the supplied start offset, and leaves later bytes untouched. A zero primary count consumes zero bytes. Failure is transactional.

## Reconstructed implementation

`enemy_actions_wbg_prefix.{h,cpp}` now implements:

- the 16-byte prefix;
- the complete primary variable-length record;
- Section B fixed compact records;
- Section C signed-outer/unsigned-inner nested groups;
- Section D 6/20 version-gated groups and their complete variable-length records;
- Section E signed counted fixed 53-byte records;
- Section F fixed 24-byte tuples repeated exactly `action_frame_count` times;
- one-byte string separators, signed-length rejection, and the evidence-derived `0xff` payload cap for string-bearing sections;
- exact signed nonpositive-count behavior and pre-bounded positive/fixed counts;
- transactional outputs for truncation or malformed lengths.

The implementation uses the project-owned bounds-checked `hp_data::Reader` / `Cursor`; it does not call the original library and does not require `CCFileUtils`, `AppParameters`, or Cocos object construction.

The final bounded stream step is Section G: one `int32` per primary record at `0x2903f2`. If a corresponding primary `ActionFrameData` exists, the original converts the integer to float and stores `1.0f / value` at `ActionFrameData + 0x5c`. Structurally, the remaining serialized tail is therefore exactly `action_frame_count * 4` bytes; reconstruction should keep the raw integers and leave the reciprocal application to a later object-construction layer.
