# TrapActions full document — 2026-10-12

## Scope

This note composes the already recovered `TrapActionsData` serialized sections into one bounded project-owned parser. No proprietary game file, decoded asset, screenshot, heap dump, or disassembly listing is committed.

## Recovered order

The current evidence-backed stream order is:

1. fixed prefix plus primary `ActionFrameData` array;
2. Trap-specific unsigned compact block;
3. nested ActionFrameData groups;
4. versioned ActionFrameData groups (6 or 20 from the recovered version gate);
5. fixed-tail ActionFrameData block;
6. one 24-byte combo tuple per primary record;
7. one final int32 reciprocal-table entry per primary record.

The final table is wire-compatible with the already recovered EnemyActions Section G parser: each serialized int32 produces `1.0f / float(value)` for the selected primary `ActionFrameData +0x5c` destination.

## Project parser

`trap_actions_document::parse()` chains the existing bounded section parsers only through their reported `bytes_consumed` values. It uses the primary record count recovered from the Trap prefix for both the combo section and the final reciprocal table.

The parse is transactional: truncation of the final table does not publish a partially parsed core/combo document. Extra trailing bytes are reported rather than rejected because an exact original EOF rejection policy is not required by the recovered loader path.

## Runtime boundary

This completes the currently recovered serialized `TrapActionsData::loadWBGFile()` document structure. It does not yet instantiate `SceneActionsSystem`, select `se01_bornXX/se01_iXX` playback, or render the `klhuo` scene action. Those are runtime consumers of this parsed data and remain separate reconstruction work.
