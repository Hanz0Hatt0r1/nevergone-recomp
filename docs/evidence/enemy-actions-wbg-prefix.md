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

Each primary record has this fixed prefix:

| Relative offset | Native call site | Type |
| ---: | ---: | --- |
| `0` | `0x28f538` | `int32` |
| `4..48` | `0x28f54e..0x28f636` | 12 × `float` |
| `52` | `0x28f650` | `bool` |
| `53` | `0x28f664` | `int32` |
| `57` | `0x28f678` | `float` |
| `61` | `0x28f68e` | first signed string length |

Three variable segments use `[int32 length][one skipped byte][length bytes]`, with char-copy calls at `0x28f6a6`, `0x28f6f4`, and `0x28f736`. The fixed size excluding payload bytes is 76, so total record size is `76 + len1 + len2 + len3`.

## Temporary string-buffer bound

The three original destination buffers begin at `sp+0xa4`, `sp+0x1a4`, and `sp+0x2a4`. Each is followed by `buffer[length] = 0`, and adjacent buffers are exactly `0x100` bytes apart. The clean-room parser caps each payload at `0xff` bytes so the explicit NUL remains inside the observed original buffer. This is a reconstruction safety bound, not evidence that the original parser validated hostile lengths.

## Section B: compact ActionFrameData block

After the primary loop:

- `0x28f842`: signed `int32` count;
- signed `BGE` loop guard at `0x28f84c..0x28f854`;
- each positive iteration reads `int32` at `0x28f870`, `float` at `0x28f882`, and `float` at `0x28f896`;
- the resulting `ActionFrameData` is appended to `EnemyActionsData + 0x8c`.

Nonpositive counts consume only the four-byte count. Positive counts consume `4 + count * 12` bytes.

## Section C: nested counted groups

Section C is a two-level counted structure:

- `0x28f8dc`: signed outer/group count;
- signed `BGE` outer guard at `0x28f8e6..0x28f8ec`;
- `0x28f904`: unsigned inner record count for each positive outer index;
- unsigned `BHS` inner guard at `0x28f91c..0x28f922`;
- records for outer index `i` append to the array pointer at `EnemyActionsData + 0x14 + 4*i`.

Each inner record consumes `72 + len1 + len2 + len3` bytes:

| Relative offset | Native call site | Type |
| ---: | ---: | --- |
| `0` | `0x28f962` | `int32` |
| `4..48` | `0x28f978..0x28fa66` | 12 × `float` |
| `52` | `0x28fa84` | `bool` |
| `53` | `0x28fa9c` | `int32` |
| `57` | `0x28fab0` | first signed string length |

The three strings use the same five-byte framing overhead and char copies at `0x28faca`, `0x28fb12`, and `0x28fb4e`. `ActionFrameData::createAFD()` runs at `0x28fb5a`, and insertion into the selected per-group array occurs at `0x28fc3e`.

The independent `enemy_actions_wbg_topology_evidence` contract proves the same Section C container topology.

## Section D: version-gated groups

After Section C the first header word controls a fixed outer fanout:

- `0x28fc5c`: compare header word 0 with `0x68`;
- values `> 0x68` select 20 groups; all other values select 6 groups;
- for every selected group, `0x28fc7c` reads one signed `int32` record count;
- `0x28fc98..0x28fc9e` uses a signed `BGE` inner guard;
- group index `i` targets the array pointer at `EnemyActionsData + 0x34 + 4*i`;
- insertion occurs at `0x28ffe8`.

A Section D record consumes `80 + len1 + len2 + len3` bytes:

| Relative offset | Native call site | Type |
| ---: | ---: | --- |
| `0` | `0x28fcda` | `int32` |
| `4..48` | `0x28fcf0..0x28fdda` | 12 × `float` |
| `52` | `0x28fdf6` | `bool` |
| `53` | `0x28fe0a` | `int32` |
| `57` | `0x28fe1e` | `uint32` |
| `61` | `0x28fe32` | `uint32` |
| `65` | `0x28fe46` | first signed string length |

The first payload begins at relative offset `70`, again proving `[int32 length][one skipped byte][payload]`. The first char copy is at `0x28fe64`; the second signed length/copy pair is at `0x28fe88` / `0x28feac`; the third is at `0x28fed2` / `0x28fee8`.

`ActionFrameData::createAFD()` runs at `0x28fef4`. The reconstructed parser preserves serialized scalar order and does not assign gameplay names to the two unsigned fields or other unresolved values. Native destination offsets are used only as evidence that the record is consumed before insertion, not as semantic labels.

### Section D safety contract

All 6 or 20 group-count fields are mandatory because the outer group count is derived from the header rather than serialized here. Signed nonpositive per-group counts consume no records. Before reserving records for a positive count, the clean-room parser reserves enough bytes for every remaining mandatory four-byte group header, then bounds the count against the remaining 80-byte minimum record size. The shared `0xff` string cap and transactional output rules remain in force.

## Reconstructed implementation

`enemy_actions_wbg_prefix.{h,cpp}` now implements:

- the 16-byte prefix;
- the complete primary variable-length record;
- Section B fixed compact records;
- Section C signed-outer/unsigned-inner nested groups;
- Section D 6/20 version-gated groups and their complete variable-length records;
- one-byte string separators, signed-length rejection, and the evidence-derived `0xff` payload cap;
- exact signed nonpositive-count behavior and pre-bounded positive allocations;
- transactional outputs for truncation or malformed lengths.

The implementation uses the project-owned bounds-checked `hp_data::Reader` / `Cursor`; it does not call the original library and does not require `CCFileUtils`, `AppParameters`, or Cocos object construction.

The next bounded format step is Section E: a signed count at `0x290018`, followed by fixed `0x35`-byte records (`int32 + 12 float32 + bool8`) appended to `EnemyActionsData + 0x84` at `0x290222`. The topology contract already records that boundary; semantic field names remain unresolved.
