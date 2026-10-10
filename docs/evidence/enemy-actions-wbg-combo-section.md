# EnemyActionsData WBG Section F combo tuples

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note isolates the bounded stream section that follows the fixed-tail ActionFrameData block reconstructed in `enemy_actions_wbg_prefix`.

## Proven stream shape

The original `EnemyActionsData::loadWBGFile` loop beginning around instruction offset `0x290234` runs exactly the primary-record count previously read into `EnemyActionsData + 0xc4`. Section F has no serialized count field of its own.

Every iteration consumes exactly `0x18` bytes:

- 4 × signed `int32`;
- 2 × `float32`.

The four integer meanings remain unresolved and therefore stay structurally named in the clean-room representation.

## Proven float destinations

For tuple index `i`, the two serialized floats are written into the two 100-entry regions already independently proven by `EnemyActionsData::initWithFile`:

- first float -> `EnemyActionsData + 0xd8 + 4*i`;
- second float -> `EnemyActionsData + 0x268 + 4*i`.

Those regions are each exactly 100 observed 4-byte elements. The reconstruction therefore rejects `primary_record_count > 100` instead of permitting writes beyond the evidence-backed object span. This is a clean-room safety bound; it is not a claim that the original game validated malformed files.

## Reconstruction

`enemy_actions_wbg_combo_section.{h,cpp}` provides a transactional parser that:

- accepts the already-parsed primary record count explicitly;
- consumes no extra count/header bytes;
- requires exactly 24 bytes per tuple;
- preserves all four integers and both floats without assigning gameplay semantics;
- derives the two exact destination byte offsets through `enemy_actions_layout_evidence`;
- pre-bounds the count against both the 100-entry destination regions and remaining serialized bytes;
- leaves the output unchanged on truncation or an out-of-range count.

The module does not construct original `ActionComboValue` objects, does not call the original library, and does not include proprietary WBG payloads.

The downstream branch behavior that may append derived combo objects to arrays at `EnemyActionsData + 0x94`, `+0x9c`, and `+0x98` remains outside this parser until its integer-driven selection rules are represented with the same evidence standard.
