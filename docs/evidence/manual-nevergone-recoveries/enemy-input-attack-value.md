# Manual reconstruction: enemy input attack value

> Address convention: executable addresses from RevEngInstr use an image base
> `0x1000` above the shipped ELF VMA. Subtract `0x1000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

`EnemyObject::inputAtkValue()` is a compact combat function selected from the
local RevEngInstr P-code corpus. Its row at `libcocos2dcpp.so:002be9d4` has 16
operations, one direct call, two loads and no branches. Those corpus metrics
make it a bounded candidate; they do not establish its meaning.

The related `EnemyObject::loadEnemyObjectFile()` row has 299 operations, of
which RevEngInstr emits 256 and marks the row truncated. The reconstruction
therefore uses only the exact field-transfer instructions below, rather than
inferring the rest of that larger initializer from an incomplete corpus row.

`EnemyActionsSystem::DamageFactor()` at `0x002bbeda` is another compact row in
the same P-code corpus: 13 operations, three loads, no calls, and no branches.
Its exact ARM/Ghidra evidence identifies the frame index, data pointer, and
`+0xd8` table offset, so the manual label records its role as
`combat_frame_factor_lookup`.

The source of `EnemyObject +0x2bc` is now also resolved in
`EnemyObject::loadEnemyObjectFile()` at `0x002be768`. Its ARM instructions at
`0x002be788..0x002be790` load `GameLevelsEnemyData +0x20`, convert it with
`vcvt.u32.f32`, and store it at `EnemyObject +0x2bc`. The preceding stores from
the same `r1` base match the independently recovered `GameLevelsEnemyData
+0x18` health source. The raw `LoadGL_Global` field corresponding to
`GameLevelsEnemyData +0x20` is `EnemyRecord.first_float`: the original parser
reads it between `second_i32` and `third_i32`, then stores it at `+0x20` (see
`docs/evidence/enemy-record-identity-health-2026-10-15.md`).

The calculation is established by the original ARMv7 disassembly and Ghidra
decompilation for Never Gone 1.0.9 (`libcocos2dcpp.so` SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`):

1. `EnemyObject::inputAtkValue()` at `0x002be9d4` loads the unsigned value at
   `EnemyObject +0x2bc` and converts it with `vcvt.f32.u32`.
2. It loads the `EnemyActionsSystem*` at `EnemyObject +0x10c` and calls
   `EnemyActionsSystem::DamageFactor()` at `0x002bbeda`.
3. `DamageFactor()` reads one 32-bit value at
   `EnemyActionsData +0xd8 + current_frame * 4`, where the data pointer is at
   `EnemyActionsSystem +0x108` and the current frame is at `+0x294`.
4. `inputAtkValue()` multiplies the converted unsigned value by that current
   frame factor and returns the product.

`enemy_attack_value_contract::try_initialize_value_2bc()` reproduces the
unsigned conversion for finite values in the uint32 range and safely rejects
values outside that proven domain. `resolve()` implements the subsequent
unsigned-to-float conversion and multiplication. It accepts the frame factor as
an input, and `resolve_at_frame()` obtains that factor from the parsed action
document. Section F's first float at tuple index `i` is proven to populate
`EnemyActionsData +0xd8 + 4*i` (`docs/evidence/enemy-actions-wbg-combo-section.md`).

`game_levels_enemy_record_mapping::resolve_input_value_2bc()` follows the
reviewed enemy ID mapping to exactly one imported `EnemyRecord`, converts its
`first_float`, and fails closed for missing IDs, duplicate IDs, unreviewed
mappings, or values outside the defined conversion range. The tutorial actor
binding stores this recovered input alongside imported base health. The live
gate can combine it with the selected action document's current frame factor;
the remaining boundary is scheduling that result from an enemy attack event
and applying contact damage to the player.

The numeric attack-value inputs are restored and exposed through the live gate.
The current implementation still does not apply damage to the player, create
enemy AI attacks, or connect attack events to the first combat wave. Those are
the next runtime boundaries.
