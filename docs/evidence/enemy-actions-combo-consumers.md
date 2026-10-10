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
- system byte `+0x190 == 0`;
- system byte `+0x290 == 0`.

It then selects `objectAtIndex(system+0x2a0)` and compares current frame `system+0x294` against:

- `ActionComboValue+0x14` as inclusive lower bound;
- `ActionComboValue+0x18` as inclusive upper bound.

When the frame is inside the range, the original writes byte `1` to system `+0x191`, reads `ActionComboValue+0x1c`, and when that value equals integer `2` also writes byte `1` to system `+0x290`. The function returns true on this matched path and false on the gated/miss paths.

The project-owned reconstruction has two layers:

- `combo_hit_mode_for_frame()` preserves the data-only `array.count() > 1`, index and inclusive range checks and returns `field_1c` on a match;
- `apply_combo_hit_transition()` wraps that lookup with the proven byte-state gates and writes using offset-named `ComboHitState {flag_190, flag_191, flag_290}`.

The state helper intentionally keeps raw byte names. It treats any nonzero `flag_190` or `flag_290` as gating the native path, copies state unchanged on misses, writes exactly byte value `1` to `flag_191` on a match, and additionally writes `1` to `flag_290` when `field_1c == 2`. Invalid project-owned range indices are a safe no-op/false result rather than an attempt to reproduce invalid `CCArray` access.

## `EnemyActionsSystem::waUpdate()`

The original `waUpdate(float)` at `0x2ad520` independently confirms that arrays `+0x94` and `+0x98` are ordered boundary streams: it selects the current objects using system indices `+0x2a0/+0x2a4`, compares current frame `+0x294` with each object's `+0x18`, and advances/clamps those array indices when the boundary is reached.

The exact boundary update is visible at `0x2ad622..0x2ad64e` for `+0x94` and `0x2ad652..0x2ad67c` for `+0x98`:

- if `current_frame < ActionComboValue+0x18`, keep the current index;
- otherwise increment the index by one;
- compare that incremented index with `array.count()`;
- if it equals count, immediately subtract one again, leaving the externally visible index on the final element;
- for the `+0x94` path only, the function also distinguishes a real transition to a next element from the final-element clamp.

`advance_boundary_cursor()` models this data-dependent operation. It reports the resulting index, whether a real next boundary was selected, and whether the update instead hit the final-boundary clamp. Empty arrays and stale project-owned indices are left unchanged safely; this is a reconstruction safety rule, not a claim about malformed native state.

A second bounded state slice is visible immediately around that path after the current-frame update:

- if system byte `+0x290` is nonzero, native writes byte `1` to system `+0x291`;
- system byte `+0x191` gates the `+0x94` endpoint/cursor update: zero skips that cursor path, nonzero enables the already-proven endpoint check.

`apply_wa_update_combo_transition()` models only those two operations. Its neutral `WaUpdateComboState` carries `flag_191`, `flag_290`, `flag_291` and `boundary_index_2a0`. A nonzero `flag_290` normalizes `flag_291` to byte value `1`; `flag_191 == 0` preserves the +0x94 index, while nonzero `flag_191` delegates to `advance_boundary_cursor()`.

### Late post-endpoint transition

Later in the same `waUpdate()` control flow, the local result of the `+0x94` cursor path gates a second branch. When no real next-boundary transition occurred and the current frame is strictly greater than the selected `ActionComboValue+0x18` endpoint, native performs the following writes:

- `system+0x18c = endpoint - 1`;
- `system+0x190 = 1`;
- `system+0x294 = 0`;
- `system+0x1bc = 1`;
- if byte `system+0x292 == 0`, also `system+0x169 = 0`.

The comparison is strict: `current_frame == endpoint` does not enter this late block. A real `+0x94` transition suppresses the block even when the frame is already beyond the previous endpoint.

`apply_wa_update_completion_transition()` reconstructs exactly this offset-named write set in `WaUpdateCompletionState`. It preserves `+0x169` when `+0x292` is nonzero and models `endpoint - 1` with ARM32 wraparound. The independent `+0x98` stream, calls reached on the alternate `updateData()` path, animation/timing work and any gameplay interpretation of these state fields remain outside this contract.

Together these helpers establish a contiguous structural bridge from a `comboHit()` mode-2 match (`+0x290 = 1`) through the next `waUpdate()` byte propagation (`+0x291 = 1`), boundary progression, and the proven late post-endpoint reset writes without assigning speculative semantic names.

No proprietary WBG data or original source code is included in this reconstruction.
