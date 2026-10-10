# EnemyActions secondary action-frame entry

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Section-B storage mapping

The native WBG loader at `0x28f842..0x28f8be` reads each 12-byte Section-B record as `[int32, float32, float32]`, creates an `ActionFrameData`, and stores:

- serialized int32 -> `ActionFrameData+0x74`;
- first float -> `ActionFrameData+0x14`;
- second float -> `ActionFrameData+0x18`;
- the object is appended to `EnemyActionsData+0x8c`.

This matches the project `CompactActionFrameRecord` layout.

## Runtime entry

The recovered runtime slice is `0x2ac83c..0x2ac86e`.

Native performs:

1. require `system+0x108` (EnemyActionsData) non-null;
2. require `EnemyActionsData+0x8c` non-null;
3. call `CCArray::count()` and require count nonzero;
4. call `objectAtIndex(system+0x294)` with the raw 32-bit current-frame value;
5. read `ActionFrameData+0x18` from the selected secondary record.

There is no additional current-frame range check in this local slice.

## Project-owned reconstruction

`enemy_actions_secondary_entry.h` reports native lookup intent separately from safe project-owned record availability. If the native path would call `objectAtIndex()` but the parsed vector index is unavailable, the helper sets `native_would_object_at_index=true` while leaving `selected_record_available=false`; it does not emulate an invalid container access.

When available, `selected_field_18` is exactly the second float from the corresponding Section-B record.

The following sprite-frame lookup and `system+0x114` property updates remain outside this entry contract.

No proprietary payload or decompiler-derived source is included.
