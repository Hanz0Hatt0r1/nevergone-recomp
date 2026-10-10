# EnemyActionsSystem combo consumers

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records the first downstream uses of the `ActionComboValue` arrays reconstructed from WBG Section F. It does not assign broader gameplay meanings to unresolved fields.

## `EnemyActionsSystem::curFramePower()`

The original function at `0x2adb3c` reads `EnemyActionsData*` from system `+0x108` and current frame index from system `+0x294`, then accesses the `ActionComboValue` array at `EnemyActionsData+0x9c`.

Behavior:

- if `current_frame_index >= array.count()`, return integer `1`;
- otherwise `objectAtIndex(current_frame_index)`;
- if the object pointer is null, return `1`;
- otherwise return the 32-bit field at `ActionComboValue+0x1c`.

Because the reconstructed Section F emitter produces one non-null structural value per primary tuple, `enemy_actions_combo_consumer::current_frame_power()` mirrors the data-dependent portion as an indexed `field_1c` lookup with fallback `1`.

## `EnemyActionsSystem::comboHit()`

The original function at `0x2abe16` consumes the array at `EnemyActionsData+0x94`.

Before testing a range it requires:

- `array.count() > 1`;
- two external `EnemyActionsSystem` byte flags (`+0x190` and `+0x290`) to be zero.

It then selects `objectAtIndex(system+0x2a0)` and compares current frame `system+0x294` against:

- `ActionComboValue+0x14` as inclusive lower bound;
- `ActionComboValue+0x18` as inclusive upper bound.

When the frame is inside the range, the original sets an internal hit flag and reads `ActionComboValue+0x1c`; equality with integer `2` sets another system flag.

The project-owned helper `combo_hit_mode_for_frame()` intentionally models only this data-dependent portion:

- it preserves the native `array.count() > 1` gate;
- validates the requested reconstructed range index;
- applies the inclusive `[field_14, field_18]` range check;
- returns `field_1c` when the frame matches.

The two external system flags and their mutations remain outside this helper until the surrounding EnemyActionsSystem state machine is reconstructed.

## `EnemyActionsSystem::waUpdate()`

The original `waUpdate(float)` at `0x2ad520` independently confirms that arrays `+0x94` and `+0x98` are ordered boundary streams: it selects the current objects using system indices `+0x2a0/+0x2a4`, compares current frame `+0x294` with each object's `+0x18`, and advances/clamps those array indices when the boundary is reached.

The exact boundary update is visible at `0x2ad622..0x2ad64e` for `+0x94` and `0x2ad652..0x2ad67c` for `+0x98`:

- if `current_frame < ActionComboValue+0x18`, keep the current index;
- otherwise increment the index by one;
- compare that incremented index with `array.count()`;
- if it equals count, immediately subtract one again, leaving the externally visible index on the final element;
- for the `+0x94` path only, the function also distinguishes a real transition to a next element from the final-element clamp.

`advance_boundary_cursor()` models this data-dependent operation. It reports the resulting index, whether a real next boundary was selected, and whether the update instead hit the final-boundary clamp. Empty arrays and stale project-owned indices are left unchanged safely; this is a reconstruction safety rule, not a claim about malformed native state.

This establishes the structural role of `field_18` as an end-index boundary without yet assigning gameplay names to the two streams. The surrounding timing, hit flags and action-completion state in `waUpdate()` remain outside this pure helper.

No proprietary WBG data or original source code is included in this reconstruction.
