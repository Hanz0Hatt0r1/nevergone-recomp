# TrapActionsData primary records — 2026-10-12

## Clean-room scope

Static inspection of the shipped ARMv7 `TrapActionsData::loadWBGFile()` establishes that the `record_count` from the fixed 16-byte prefix controls a sequential array of primary `ActionFrameData` records beginning at offset `0x10`.

## Wire-format compatibility

The primary TrapActions record uses the same byte sequence already recovered for the primary EnemyActions `ActionFrameRecord`:

1. signed int32;
2. twelve float32 values;
3. one bool byte;
4. signed int32;
5. one float32;
6. three repeated string fields, each encoded as signed int32 length, one framing byte, then exactly `length` bytes.

This equivalence is not inferred from class names. The TrapActions native read sequence and subsequent `ActionFrameData` field assignments match the existing bounded EnemyActions primary parser field-for-field, including all three one-byte string framing values.

For zero-length string payloads the record consumes the existing `kActionFrameFixedBytes = 76` bytes. Variable payload lengths are added to that exact fixed size.

## Project implementation

`trap_actions_primary::parse()` now:

- validates the TrapActions fixed prefix;
- rejects declared record counts that cannot fit even the minimum primary-record width in the remaining input;
- delegates each individual record to the established transactional `enemy_actions_wbg_prefix::parse_action_frame_record()` implementation;
- advances by each proven `bytes_consumed` value;
- publishes the block only after every declared primary record parses successfully.

This reuse avoids maintaining two subtly divergent binary parsers for an identical wire record.

## Runtime relevance

`SceneActionsSystem::initWithFile()` accesses the first primary record through the `TrapActionsData` primary array, loads its first string as a sprite-frame plist and resolves its third string as the initial sprite frame. Parsing the complete primary block therefore establishes the first real payload needed for project-owned `klhuo` born/idle SceneActions construction.

Nested secondary/sub-action/effect sections that follow this primary array remain separate format boundaries and are not consumed by this increment.
