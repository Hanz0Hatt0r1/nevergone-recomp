# TrapActionsData compact block — 2026-10-12

## Clean-room scope

Immediately after the primary `ActionFrameData` array, the shipped ARMv7 `TrapActionsData::loadWBGFile()` reads another four-byte count and iterates a compact ActionFrameData block. This note records only that proven binary boundary.

## Header signedness

The count is loaded through the unsigned `HPData::getBytes(unsigned int&, HPRange)` overload. Native then compares the zero-based loop index against the loaded count with the ARM unsigned `BHS` condition.

Therefore this field is a **uint32 count**. Values with bit 31 set are not negative/empty sentinels. A project parser must reject such a count when the declared payload cannot fit rather than silently treating it as zero records.

## Record layout

Each iteration consumes exactly 12 bytes:

| Relative offset | Width | Field |
| ---: | ---: | --- |
| `+0x00` | 4 | signed int32 |
| `+0x04` | 4 | float32 |
| `+0x08` | 4 | float32 |

Native creates one `ActionFrameData`, copies those values to `+0x74`, `+0x14`, and `+0x18`, and appends it to the `TrapActionsData +0x8c` array.

The 12-byte record body is byte-compatible with the existing EnemyActions compact record structure, but the block header semantics differ: the EnemyActions helper currently models a signed count, while TrapActions is proven unsigned. For that reason `trap_actions_compact::parse()` reuses only the record structure/constants and owns its own uint32 header/loop validation.

## Project boundary

The parser:

- accepts an explicit stream offset, normally `trap_actions_primary::Block::bytes_consumed`;
- reads the uint32 count transactionally;
- bounds the count against the remaining bytes divided by the exact 12-byte minimum record size;
- consumes each declared record in order;
- reports exact bytes consumed as `4 + count * 12`.

No semantics beyond the observed ActionFrameData destinations are assigned yet. The following TrapActions sections remain separate reconstruction boundaries.
