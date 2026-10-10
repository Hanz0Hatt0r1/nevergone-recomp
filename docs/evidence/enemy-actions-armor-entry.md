# EnemyActions armor entry/reset slice

Original target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note covers only the armor reset and record-selection slice inside `EnemyActionsSystem::updateActionFrameMoveValue()`.

## Eight armor sprite slots are hidden first

At `0x2aca6c..0x2aca8a` native walks eight pointers starting at `system+0x130` with a four-byte stride and, for every non-null pointer, invokes the virtual `setVisible(false)` slot. The visited offsets are therefore:

`+0x130, +0x134, +0x138, +0x13c, +0x140, +0x144, +0x148, +0x14c`.

The reconstruction reports these visibility requests; it does not model cocos2d object pointers.

## Armor arrays and record order

After the reset, `r9` starts at zero. Native calls `EnemyActionsData::armorArrayWithID(r9)` (`0x290552`), checks `count()`, and skips directly to the next armor id when the count is zero.

For a non-empty array, `r8` starts at zero. The loop repeatedly calls `EnemyActionsData::armorFDWithID(r9,r8)` (`0x29055a`) and compares:

- selected `ActionFrameData+0x74` (`0x2acae2`)
- against `system+0x294` current frame (`0x2acade`).

A mismatch increments `r8` and continues. Once the later armor body completes for a matching record, control joins the next-id path rather than continuing the same array, so the first matching record in ascending index order is the one that reaches that body.

The outer id advances at `0x2acdfa`; `0x2ace00` compares it with `8`, proving exactly eight armor ids are visited.

## Section-C mapping

The existing WBG topology reconstruction maps Section C group `i` to `EnemyActionsData + 0x14 + 4*i`. Each parsed `NestedActionFrameRecord::first_i32` is the serialized first `int32`; native construction stores that value into `ActionFrameData+0x74`. Therefore the project-owned bounded equivalent of the native equality test is:

`nested_block.groups[id].records[index].first_i32 == current_frame_294`.

The native object allocates its CCArray slots independently of serialized group presence. The helper therefore treats a missing parsed group as an empty/unavailable project representation rather than dereferencing beyond the parsed vector.

## Deliberate boundary

This contract stops immediately after selection. It does not reconstruct:

- path creation from `system+0x250/+0x268`;
- sprite-frame plist loading or resource recording;
- armor frame-name lookup;
- `system+0x130+4*id` transform/property writes;
- later root/tail arrays.

No proprietary payload or decompiler-derived source is committed.
