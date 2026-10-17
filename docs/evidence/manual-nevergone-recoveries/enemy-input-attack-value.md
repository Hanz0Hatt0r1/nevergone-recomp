# Manual reconstruction: enemy input attack value

`EnemyObject::inputAtkValue()` is a compact combat function selected from the
local RevEngInstr P-code corpus. Its row at `libcocos2dcpp.so:002be9d4` has 16
operations, one direct call, two loads and no branches. Those corpus metrics
make it a bounded candidate; they do not establish its meaning.

The related `EnemyObject::loadEnemyObjectFile()` row has 299 operations, of
which RevEngInstr emits 256 and marks the row truncated. The reconstruction
therefore uses only the exact field-transfer instructions below, rather than
inferring the rest of that larger initializer from an incomplete corpus row.

The source of `EnemyObject +0x2bc` is now also resolved in
`EnemyObject::loadEnemyObjectFile()` at `0x002be768`. Its ARM instructions at
`0x002be788..0x002be790` load `GameLevelsEnemyData +0x20`, convert it with
`vcvt.u32.f32`, and store it at `EnemyObject +0x2bc`. The preceding stores from
the same `r1` base match the independently recovered `GameLevelsEnemyData
+0x18` health source. The raw `LoadGL_Global` field corresponding to
`GameLevelsEnemyData +0x20` is not yet established.

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
an input because the `EnemyActionsData +0xd8` table and runtime import path are
not yet connected to the tutorial actor.

This restores the numeric function only. It does not apply damage to the
player, create enemy AI attacks, or connect attack events to the first combat
wave. Those are the next runtime boundaries.
