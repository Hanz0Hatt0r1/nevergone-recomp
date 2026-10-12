# TrapActionsData core sections — 2026-10-12

## Clean-room scope

This note extends the recovered `TrapActionsData::loadWBGFile()` boundary after the primary and compact arrays. The next three serialized sections are structurally identical to already recovered EnemyActions WBG blocks, so project code composes those proven bounded parsers instead of cloning them.

## Nested block

Immediately after the unsigned Trap compact block, native reads a **signed int32 outer group count**. The outer loop uses signed `BGE`; zero or negative values consume no groups.

For every positive outer group, native reads a **uint32 record count** and compares the zero-based record index with unsigned `BHS`. Each nested record is:

- signed int32;
- twelve float32 values;
- bool8;
- signed int32;
- three framed string fields (`int32 length + uint8 framing + payload`).

This is byte-compatible with `enemy_actions_wbg_prefix::NestedActionFrameBlock`.

## Versioned group block

After nested groups, native chooses a fixed number of groups from the Trap file's first prefix int32:

- `first_i32 > 0x68` → 20 groups;
- otherwise → 6 groups.

Each group begins with a signed int32 record count. Positive records use the same versioned ActionFrameData wire layout already reconstructed for EnemyActions, including two uint32 fields before the three framed strings. This matches `VersionedActionFrameBlock` and the existing `versioned_group_count_for_header()` rule.

## Fixed-tail block

After the 6/20 versioned groups, native reads one signed int32 count. Positive iterations each consume exactly:

- signed int32;
- twelve float32 values;
- bool8;

for `0x35` bytes per record. This matches `FixedTailActionFrameBlock`.

## Project composition

`trap_actions_core_document::parse()` now consumes, in order:

1. Trap prefix + primary ActionFrameData array;
2. Trap-specific unsigned compact block;
3. compatible nested block;
4. compatible versioned 6/20 group block;
5. compatible fixed-tail block.

It advances solely by each transactional parser's reported `bytes_consumed`, publishes output only after the complete recovered sequence succeeds, and reports remaining bytes rather than claiming EOF. Those remaining bytes begin the subsequent per-primary-record combo section and later tables, which remain separate reconstruction boundaries.
