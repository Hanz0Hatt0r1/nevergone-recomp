# EnemyActionsData pre-parser initialization contract

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records only writes proven in Thumb disassembly of `EnemyActionsData::initWithFile(cocos2d::CCString*)` at ELF instruction offset `0x29043c`. It deliberately stops at the call to `EnemyActionsData::loadWBGFile` at `0x29054a`; no WBG field meanings are inferred.

## Scalar writes before parsing

The function writes these 32-bit values before any WBG parser work:

| Object offset | Raw value |
|---:|---:|
| `+0xa4` | `0x00000000` |
| `+0xa8` | `0x00000001` |
| `+0xac` | `0x00000001` |
| `+0xb0` | `0x00000001` |
| `+0xb4` | `0x00000001` |
| `+0xb8` | `0x00000001` |
| `+0xbc` | `0x00000001` |
| `+0xc0` | `0x00000018` |
| `+0xc8` | `0x00000000` |
| `+0xcc` | `0x00000000` |
| `+0xd0` | `0x00000000` |
| `+0xd4` | `0x00000000` |

`+0xc4` is not initialized in this pre-parser block. It is one of the early destinations later written by `loadWBGFile`, so assigning it a default here would be unsupported.

## CCArray construction

`initWithFile` creates and retains a `CCArray` with capacity `0x80` for every pointer-sized slot from `this+0x14` through `this+0xa0`, inclusive, in 4-byte steps. That is 36 array slots total.

The machine code reaches those slots through four explicit stores (`+0x84..+0x90`), an eight-entry loop (`+0x14..+0x30`), a twenty-entry loop (`+0x34..+0x80`), and four final explicit stores (`+0x94..+0xa0`). The runtime contract stores the normalized sorted slot set; it does not assign semantic names to the arrays.

## Two 100-entry regions

Immediately before `loadWBGFile`, one loop performs 100 iterations and writes:

- `0x3f800000` (`1.0f`) to `this+0xd8 + i*4`;
- `0x00000000` to `this+0x268 + i*4`;
- for `i = 0..99`.

The first region therefore ends at `+0x264`, the second starts at `+0x268`, and the observed object span reaches through `+0x3f4`, requiring at least `0x3f8` bytes.

## Boundary

These are initialization writes and allocation geometry only. They do not establish the semantic meaning of the scalar fields, arrays, or per-entry regions. WBG serialization order remains unresolved until a real user-owned WBG file is available to the bounded unidbg harness.
