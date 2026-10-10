# EnemyActionsData WBG Section F combo tuples

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note isolates the bounded stream section that follows the fixed-tail ActionFrameData block reconstructed in `enemy_actions_wbg_prefix`.

## Proven stream shape

The original `EnemyActionsData::loadWBGFile` loop beginning around instruction offset `0x290234` runs exactly the primary-record count previously read into `EnemyActionsData + 0xc4`. Section F has no serialized count field of its own.

Every iteration consumes exactly `0x18` bytes:

- 4 × signed `int32`;
- 2 × `float32`.

The four integer gameplay meanings remain unresolved and therefore stay structurally named in the clean-room representation.

## Proven float destinations

For tuple index `i`, the two serialized floats are written into the two 100-entry regions already independently proven by `EnemyActionsData::initWithFile`:

- first float -> `EnemyActionsData + 0xd8 + 4*i`;
- second float -> `EnemyActionsData + 0x268 + 4*i`.

Those regions are each exactly 100 observed 4-byte elements. The reconstruction therefore rejects `primary_record_count > 100` instead of permitting writes beyond the evidence-backed object span. This is a clean-room safety bound; it is not a claim that the original game validated malformed files.

## ActionComboValue initialization

`ActionComboValue::initACV()` at `0x48c79e` initializes the four 32-bit object fields used by this loop as:

- object `+0x14 = 0`;
- object `+0x18 = 1`;
- object `+0x1c = 1`;
- object `+0x20 = 0`.

`createACV()` allocates `0x24` bytes and calls the virtual initializer before autorelease. The reconstruction records these fields by byte offset rather than assigning gameplay names.

## Proven derived-object emission

The Section F loop increments serialized integer fields 1, 2 and 3 by one immediately after reading them. The additions use normal ARM32 integer arithmetic, so the clean-room implementation preserves modulo-`2^32` wrapping explicitly.

Three independent `CCArray` targets are then populated:

### `EnemyActionsData + 0x9c`

One `ActionComboValue` is emitted for every tuple (`0x290374..0x290384`). The object keeps initializer defaults except:

- object `+0x1c = tuple.i32_values[2] + 1`.

### `EnemyActionsData + 0x94`

The loop starts with current state `1` and previous boundary index `-1`. It compares current state with `tuple.i32_values[3] + 1`.

When the value changes, or when the current tuple is the final primary tuple, an `ActionComboValue` is emitted (`0x29033c..0x290368`) with:

- object `+0x14 = previous_boundary_index + 1`;
- object `+0x18 = current tuple index`;
- object `+0x1c = current state`;
- object `+0x20` remains the initializer default `0`.

After emission, the previous boundary index becomes the current tuple index and current state becomes the incremented fourth integer.

### `EnemyActionsData + 0x98`

The equivalent boundary logic at `0x290388..0x2903b4` is driven by `tuple.i32_values[1] + 1`, again starting from current state `1` and previous boundary index `-1` and always closing the final run.

The first serialized integer is not used by the recovered ActionComboValue emission sequence and remains structurally preserved only.

## Reconstruction

`enemy_actions_wbg_combo_section.{h,cpp}` provides a transactional parser that:

- accepts the already-parsed primary record count explicitly;
- consumes no extra count/header bytes;
- requires exactly 24 bytes per tuple;
- preserves all four integers and both floats without assigning gameplay semantics;
- derives the two exact destination byte offsets through `enemy_actions_layout_evidence`;
- reconstructs the exact observed `ActionComboValue` field writes and target-array selection for `+0x94`, `+0x98` and `+0x9c`;
- pre-bounds the count against both the 100-entry destination regions and remaining serialized bytes;
- leaves the output unchanged on truncation or an out-of-range count.

The module does not instantiate original Cocos objects, call the original library, or include proprietary WBG payloads. Gameplay meanings of the integer fields and downstream consumers of the three combo arrays remain unresolved.
