# EnemyActions cut source selection

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records the bounded source-selection slice reached only after the earlier `+0x250/vtable+0xcc` retained-object gate is open.

## Native branch on `system+0x278`

At `0x2ac494..0x2ac4a4` native loads float `system+0x278`, compares it with zero using `VCMPE`, transfers FPSCR flags to APSR, and takes the EnemyObject-owned branch only with `BGT`.

Consequences:

- strictly positive `+0x278` -> EnemyObject-owned cut array;
- zero or negative `+0x278` -> system default `+0x27c` array;
- unordered/NaN compare does not satisfy `BGT`, so it also takes the default-array branch.

## Positive branch: EnemyObject-owned cuts

At `0x2ac4a6..0x2ac4e4` native repeatedly calls `EnemyObject::getEnemyActionCutObjectArray()` on the retained object from `vtable+0xcc`.

For each entry in array order it:

1. loads `ActionsCut+0x14`;
2. reads current `ActionFrameData+0x68`;
3. compares the two strings;
4. on equality branches to the existing-cut path at `0x2ad284`.

The scan is ordered, so duplicate frame keys resolve to the first matching `ActionsCut`.

If no match is found, control reaches `0x2ac550`. It does **not** fall back to `system+0x27c`; instead it enters the separate unmatched-enemy-cut path whose later sprite-frame checks may create a new EnemyObject-owned `ActionsCut`.

The matched `ActionsCut` layout is independently bounded by `ActionsCut::init()` at `0x48c790`:

- `+0x14` pointer/string slot;
- `+0x18` float/word slot;
- `+0x1c` float/word slot;
- all three initialized to zero.

The existing-cut branch later reads both `+0x18` and `+0x1c`, but their arithmetic effects are intentionally outside this source-selection contract.

## Non-positive/default branch

At `0x2ac4e6..0x2ac518` native scans `system+0x27c` from index zero. The array's construction contract is recovered separately in `enemy-actions-default-cut-table.md`.

For each default `ActionsCut` it compares `ActionsCut+0x14` with current `ActionFrameData+0x68`.

- first equality -> default rect-patch path at `0x2ac518`;
- no equality through array end -> common downstream join at `0x2ad2f6`;
- this branch never consults the EnemyObject-owned array.

The matched default entry contributes `ActionsCut+0x1c`, which is the stored sprite-frame rect height. The actual `CCSpriteFrame::setRect()` mutation is a later bounded contract and is not performed by this helper.

## Project-owned reconstruction

`enemy_actions_cut_selection.h` exposes:

- `EnemyCutObservation { frame_name_14, field_18, field_1c }`;
- explicit source enum: none / EnemyObject / system default;
- `select(cut_processing_gate_open, field_278, current_frame_name_68, enemy_cuts, default_cuts)`.

The result reports:

- which native array was selected;
- whether a first match was found and at what index;
- the selected offset-faithful cut values;
- whether control takes the existing-enemy-cut path, unmatched-enemy-cut path, default rect-patch path, or default no-match path.

If the earlier retained-object gate is closed, this helper performs no cut-array selection because native jumps around the entire slice.

## Corrected prior interpretation

Earlier evidence described the null `vtable+0xcc` result as bypassing a broad object-update block. The expanded trace shows that this was too broad: null bypasses this cut-processing slice but rejoins the common downstream transform/update path at `0x2ac600`. The runtime object-gate helper and its evidence note are corrected in the same change.

## Outside scope

This contract does not yet reconstruct:

- the existing-enemy-cut `CCSpriteFrame::setRect()` arithmetic;
- new EnemyObject-owned `ActionsCut` creation on the unmatched path;
- the default-cut `setRect()` write;
- downstream sprite position, scale, visibility, flip, transform or render-state calls.

No proprietary payload or decompiler-derived source is included.
