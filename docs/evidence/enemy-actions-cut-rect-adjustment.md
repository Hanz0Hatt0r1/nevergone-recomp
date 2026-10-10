# EnemyActions cut rect adjustment

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records the bounded rect-height arithmetic shared by the EnemyObject-owned existing-cut path and the unmatched path that creates a new EnemyObject-owned `ActionsCut`.

## Native arithmetic

The cut-processing trace shows the mode-0 retained EnemyObject supplies `EnemyObject::actionWithHPointOffset()`. Native compares that float against system float `+0x280` using `VCMPE` and the transferred ARM condition flags.

The observed result is equivalent for finite inputs to:

- when `action_hpoint_offset < system+0x280`: `delta = system+0x280 - action_hpoint_offset`;
- otherwise: `delta = 0`.

For unordered/NaN comparison, the condition used for the subtraction is not taken, so `delta` remains zero.

The selected/current sprite-frame rect is copied before mutation. Its fourth float (the same rect-height slot independently observed as `CCSpriteFrame+0x44`) is then replaced with:

`ActionsCut+0x1c - delta`

and native calls `CCSpriteFrame::setRect()` with the modified rect.

For EnemyObject-owned cuts, native also stores:

`ActionsCut+0x18 = delta * 0.5f`

The trace does not show a clamp after subtracting `delta` from `ActionsCut+0x1c`; a negative fourth rect float is therefore preserved by the clean-room arithmetic rather than normalized.

## Existing vs new EnemyObject cuts

For an existing EnemyObject-owned cut, `ActionsCut+0x1c` is the already stored original fourth rect float and `ActionsCut+0x18` is overwritten with the newly computed half-delta.

For the unmatched/new-cut path, native captures the current sprite-frame fourth rect float before reducing it, then creates `ActionsCut` and stores:

- `+0x14` = retained frame-name string;
- `+0x1c` = original fourth rect float before adjustment;
- `+0x18` = `delta * 0.5f`.

The actual new-cut allocation/append side effect is intentionally a separate contract. This helper only reconstructs the shared arithmetic needed by both paths.

## System-default path

The matched `system+0x27c` path is simpler: native copies the current sprite-frame rect, replaces only the fourth float with matched default `ActionsCut+0x1c`, and calls `setRect()`. It does not apply the H-point subtraction in this path.

`enemy_actions_cut_rect::patch_default_cut()` models only that fourth-float replacement.

## Project-owned reconstruction

`enemy_actions_cut_rect.h` exposes:

- `adjust_enemy_cut(original_rect_height, field_280, action_hpoint_offset)`;
- `patch_default_cut(cut_field_1c)`.

The EnemyObject adjustment result reports the native offset-visible values:

- input `field_280`;
- observed `action_hpoint_offset`;
- computed `delta`;
- `cut_field_18 = delta / 2`;
- preserved `cut_field_1c = original_rect_height`;
- patched rect height `original_rect_height - delta`;
- whether the strict comparison applied a nonzero subtraction.

`State.field_280` is retained with an offset-based name; no gameplay meaning beyond its proven role in this arithmetic is assigned.

## Outside scope

This contract does not perform cocos2d `getRect()`/`setRect()` calls, does not allocate or retain `ActionsCut`, does not append to the EnemyObject-owned cut array, and does not reconstruct downstream position/scale/visibility/flip updates.

No proprietary payload or decompiler-derived source is included.
