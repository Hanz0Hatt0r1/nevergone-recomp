# EnemyActions composed runtime state

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This layer composes recovered `EnemyActionsSystem::comboHit()` and bounded `waUpdate()` transitions into one offset-named project-owned state object so the reconstruction can consume them as runtime behavior rather than only as isolated helpers.

## State

`enemy_actions_runtime_state::State` retains only offsets used by recovered contracts:

- bytes `+0x168`, `+0x169`;
- int32 `+0x154` previous-frame storage;
- float32 `+0x158`, `+0x15c`, `+0x160`;
- int32 `+0x18c`;
- bytes `+0x190`, `+0x191`;
- byte `+0x1bc`;
- bytes `+0x290`, `+0x291`, `+0x292`;
- current frame int32 `+0x294`;
- `+0x94` boundary index `+0x2a0`;
- `+0x98` boundary index `+0x2a4`.

Names remain offset-based because gameplay semantics for these fields are not fully proven.

## `apply_combo_hit()`

The runtime wrapper supplies `State.boundary_index_2a0` and `State.current_frame_294` to the recovered `comboHit()` transition and maps the proven `+0x190/+0x191/+0x290` writes back into the same state. All unrelated fields are preserved.

Thus the runtime state directly preserves the already recovered native gates and inclusive `ActionComboValue+0x14..+0x18` range test without duplicating them.

## `apply_wa_update_timing()`

Fresh Thumb disassembly of `EnemyActionsSystem::waUpdate(float)` at `0x2ad520` establishes the timing gate immediately before frame processing.

The exact sequence is visible at `0x2ad528..0x2ad5c0`:

1. byte `system+0x1bc` is tested first; nonzero exits the function before timing state changes;
2. if byte `system+0x168` is nonzero, native clears `+0x168` and stores zero to word/float storage `+0x160`;
3. otherwise native loads float `+0x160`, multiplies the input delta by literal float `1000.0f` (`0x447a0000`) and accumulates it into `+0x160`;
4. it selects the current `ActionFrameData` from `EnemyActionsData+0x88` using frame index `system+0x294`;
5. float `ActionFrameData+0x5c` is added to float `system+0x15c` and stored at `system+0x158`;
6. `system+0x160` is compared with that threshold; if the accumulator is below the threshold, native returns;
7. byte `system+0x169` must be nonzero; zero returns without subtracting the threshold;
8. otherwise native subtracts `system+0x158` from `system+0x160` and continues into frame processing.

Section G had already independently established that each per-primary serialized int is converted to `1.0f / float(value)` and written to `ActionFrameData+0x5c`. Therefore the reconstructed `final_table.entries[i].reciprocal_value` is the exact project-owned source for the `+0x5c` value used by this timing gate.

`apply_wa_update_timing()` models this sequence without assigning semantic names to `+0x158/+0x15c/+0x160`. For malformed project-owned state, negative/out-of-range frame indices stop safely after the accumulator/reset step rather than attempting invalid `CCArray` access. ARM `VCMPE` followed by `BLT` treats unordered/NaN inputs as taking the early branch; the helper mirrors that explicitly.

## `apply_wa_update_after_frame_advance()`

This lower-level helper consumes a `State.current_frame_294` value after frame increment/selection has occurred. It then:

1. captures the selected pre-transition `+0x94` object's `field_18` endpoint when the current `+0x2a0` index is valid;
2. applies the recovered `+0x290 -> +0x291` propagation;
3. applies the `+0x191`-gated `+0x94/+0x2a0` boundary update;
4. applies the independent `+0x98/+0x2a4` boundary update;
5. when the pre-transition `+0x94` endpoint was valid, applies the recovered strict post-endpoint completion/reset transition using that captured endpoint and the real-advance result from the `+0x94` cursor.

If reconstructed `+0x94` state is invalid, this lower-level project-owned layer safely skips only the late completion/reset operation. The public frame-step helper below additionally reconstructs the native no-`+0x94` fallback.

## `apply_wa_update_frame_step()`

The same native trace establishes the frame-processing step after the timing gate has allowed work to continue.

For the normal path with a selected `EnemyActionsData+0x94` object, the current frame is incremented and stored at `system+0x294` around `0x2ad604..0x2ad612`; the already reconstructed `+0x94/+0x98` boundary and post-endpoint logic follows immediately.

When no current `+0x94` object exists, the separate fallback at `0x2ad6b0..0x2ad716` is used:

1. native reads the `EnemyActionsData+0x88` `ActionFrameData` array count;
2. increments and stores `system+0x294`;
3. still processes the independent `+0x98/+0x2a4` boundary stream;
4. compares the incremented frame with the `+0x88` count;
5. when `current_frame > count`, stores `count - 1` at `+0x18c`, sets `+0x190 = 1`, resets `+0x294 = 0`, sets `+0x1bc = 1`, and clears `+0x169` only when `+0x292 == 0`.

`apply_wa_update_frame_step()` reconstructs both branches. The frame increment uses raw ARM32 wrapping semantics, so `INT32_MAX` advances to `INT32_MIN` rather than invoking signed-overflow behavior in C++.

### `+0x154` / `updateData()` gate

The tail at `0x2ad71a..0x2ad738` compares previous-frame storage `system+0x154` with current frame `system+0x294`.

- if they are equal, native skips both `updateData()` and the redundant `+0x154` store path;
- when they differ, a local native gate controls the `updateData()` call;
- ordinary non-reset processing keeps the gate enabled;
- a reset with `+0x292 != 0` also keeps it enabled;
- a reset with `+0x292 == 0` disables the call;
- after a changed-frame path, native stores the current frame into `+0x154` regardless of whether `updateData()` was called.

The reconstruction reports this as `previous_frame_changed` and `should_call_update_data` and updates the offset-named `previous_frame_154`. It does not invoke or speculate about `updateData()` internals.

## `apply_wa_update_iteration()`

The project now has one directly consumable bridge from a parsed Sections A-G action-data document to the recovered runtime state. `apply_wa_update_iteration()` takes `enemy_actions_wbg_document::Document`, a delta in seconds, and the current offset-named state.

It performs exactly one composition step:

1. `document.final_table` feeds the recovered timing gate, including the Section-G-derived `ActionFrameData+0x5c` value;
2. when the timing gate does not permit frame processing, the function returns the timing-updated state without changing the frame/boundary state;
3. when frame processing is permitted, `document.combo_block` and `document.prefix.action_frame_count` feed exactly one recovered frame step;
4. the result exposes both the timing decision and the frame-step decision, including the `updateData()` call signal.

This intentionally does **not** claim to be a full `waUpdate(float)` implementation. Native control flow jumps back to the accumulator/threshold comparison after a processed frame, so one original call may consume another frame when sufficient accumulated time remains. The single-iteration helper preserves that distinction: excess `+0x160` time remains in state for a subsequent iteration instead of being consumed by an invented loop policy.

## `showActionLastFrame()`

The native `EnemyActionsSystem::showActionLastFrame()` entry at `0x2ad748` is a small independent transition. It loads int32 `system+0x18c`, stores that value directly to current frame `system+0x294`, and then transfers to `EnemyActionsSystem::updateData()` unconditionally.

`apply_show_action_last_frame()` mirrors exactly the state-visible part:

- `current_frame_294 = field_18c` with no range check or normalization;
- all other reconstructed fields are preserved, including previous-frame storage `+0x154`;
- `should_call_update_data` is always true.

The downstream call remains a signal because `updateData()` internals are not yet reconstructed.

## Explicitly outside scope

This state layer still does not model or infer:

- the internal behavior of `EnemyActionsSystem::updateData()`;
- animation, rendering, hit effects, or action dispatch;
- resets of `+0x191/+0x290/+0x291` that are not yet proven;
- gameplay-semantic names for any offset-named state;
- the full native back-edge/loop policy that may process more than one frame in one `waUpdate(float)` call.

The host coverage now includes the continuous combo/boundary chain, the native timing gate, both frame-processing branches, exact ARM32 frame increment, `+0x98` fallback behavior, count-based reset, `+0x154` bookkeeping, the recovered `updateData()` call decision, one parsed-document-driven timing→frame-step iteration, and `showActionLastFrame()`.

No proprietary payload or decompiler-derived source is included.
