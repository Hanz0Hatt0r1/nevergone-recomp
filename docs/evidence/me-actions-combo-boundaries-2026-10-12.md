# MeActions Section-F combo boundaries — 2026-10-12

## Clean-room boundary

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records bounded behavior from `MeActionsData::loadWBGFileWithHPData(HPData*)` and `MeActionsSystem::waUpdate(float)`. No original binary, disassembly listing, or proprietary action payload is committed.

## Player ActionComboValue arrays

`MeActionsData::init()` creates player-specific `CCArray` objects at `+0xb0`, `+0xb4`, `+0xb8` and `+0xbc`.

At the recovered Section-F parser loop, the same serialized `0x18`-byte tuple shape already parsed by `enemy_actions_wbg_combo_section::Block` is consumed once per primary action frame: four signed 32-bit integers followed by two floats.

The player loader derives two run-boundary arrays used by `MeActionsSystem::waUpdate()`:

- `MeActionsData +0xb0`: run state is `tuple.i32_values[3] + 1` with ARM32 wrapping;
- `MeActionsData +0xb4`: run state is `tuple.i32_values[1] + 1` with ARM32 wrapping.

Both use the same boundary emission rule:

1. runtime state starts at `1`;
2. previous boundary index starts at `-1`;
3. emit when the incremented tuple value changes, and always emit on the final tuple;
4. emitted `ActionComboValue +0x14 = previous_boundary + 1`;
5. emitted `+0x18 = current tuple/frame index`;
6. emitted `+0x1c = previous run state`;
7. update previous boundary and run state after emission.

## Relationship to the existing Section-F reconstruction

The project already reconstructs the same algorithms for `EnemyActionsData`:

- enemy `array_94_values` uses tuple int[3]+1;
- enemy `array_98_values` uses tuple int[1]+1.

The emitted `ActionComboValue` fields are therefore structurally identical to the player `+0xb0/+0xb4` values; only their destination array offsets differ because `MeActionsData` and `EnemyActionsData` have different object layouts.

`me_actions_combo_boundaries::derive()` recomputes the player values directly from `Block::tuples` and provides a structural cross-check against the already-tested enemy Section-F derivations. It does not reinterpret enemy runtime offsets as player offsets.

## `waUpdate()` consumers

`MeActionsSystem::waUpdate(float)` reads:

- selected `+0xb0` boundary using index `system+0x1f8`;
- selected `+0xb4` boundary using index `system+0x1fc`;
- boundary endpoint from `ActionComboValue+0x18`.

The player current frame is `system+0x1a4`. These arrays therefore provide the previously missing evidence-backed frame endpoints required for player frame progression and completion.

This mapping is the bridge between the already parsed Section-F tuples and the next player-specific playback step; no timing or frame advancement is added in this increment.
