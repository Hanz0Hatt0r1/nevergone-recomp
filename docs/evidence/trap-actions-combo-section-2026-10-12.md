# TrapActionsData combo section — 2026-10-12

## Clean-room scope

After the fixed-tail ActionFrameData block, `TrapActionsData::loadWBGFile()` iterates exactly the previously parsed primary-record count. There is no serialized count for this section. Each iteration consumes one 24-byte tuple and derives `ActionComboValue` objects plus two per-primary float regions.

## Wire tuple

Each tuple is:

- four signed int32 values;
- two float32 values.

The wire payload is therefore compatible with the EnemyActions combo tuple, but the TrapActions consumers differ and are reconstructed separately.

## Per-primary float destinations

For tuple index `i`, native writes:

- first float → `TrapActionsData + 0xc4 + 4*i`;
- second float → `TrapActionsData + 0x254 + 4*i`.

The project records those destination offsets structurally rather than assigning unproven semantic names.

## Derived ActionComboValue arrays

`ActionComboValue::initACV()` defaults are retained unless native overwrites a field.

### `TrapActionsData +0x94`

Native keeps state initialized to `1` and groups runs by the **raw fourth serialized int32**. It does not increment that field first. On a state change, and unconditionally for the last tuple, it emits:

- `field_14 = previous_boundary_index + 1` with 32-bit wrapping;
- `field_18 = current tuple index`;
- `field_1c = previous/current run state`.

### `TrapActionsData +0x90`

One object is appended for every tuple. Only `field_1c` is overwritten with `third serialized int32 + 1` using 32-bit wrapping.

### `TrapActionsData +0x98`

This is run-boundary logic like the `+0x94` path, but it groups the **second serialized int32 + 1**. State starts at `1`; the last tuple always closes the final run.

## Difference from EnemyActions

The raw 24-byte tuple is shared, but the TrapActions native path targets arrays `+0x94`, `+0x90`, and `+0x98`. EnemyActions targets `+0x94`, `+0x9c`, and `+0x98` and also differs in the fourth-int grouping transform. Reusing the EnemyActions combo parser would therefore corrupt the reconstructed object graph even though the bytes parse successfully.

`trap_actions_combo_section::parse()` owns this Trap-specific semantic adapter while retaining bounded, transactional byte parsing.
