# EnemyActions composed runtime state

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This layer composes recovered `EnemyActionsSystem::comboHit()` and bounded `waUpdate()` transitions into one offset-named project-owned state object so the reconstruction can consume them as runtime behavior rather than only as isolated helpers.

## State

`enemy_actions_runtime_state::State` retains only offsets used by recovered contracts:

- bytes `+0x168`, `+0x169`;
- bytes `+0x190`, `+0x191`;
- byte `+0x1bc`;
- bytes `+0x290`, `+0x291`, `+0x292`;
- int32 `+0x18c`;
- float32 `+0x158`, `+0x15c`, `+0x160`;
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

This helper consumes a `State.current_frame_294` value after frame increment/selection has occurred. It then:

1. captures the selected pre-transition `+0x94` object's `field_18` endpoint when the current `+0x2a0` index is valid;
2. applies the recovered `+0x290 -> +0x291` propagation;
3. applies the `+0x191`-gated `+0x94/+0x2a0` boundary update;
4. applies the independent `+0x98/+0x2a4` boundary update;
5. when the pre-transition `+0x94` endpoint was valid, applies the recovered strict post-endpoint completion/reset transition using that captured endpoint and the real-advance result from the `+0x94` cursor.

If reconstructed `+0x94` state is invalid, the project-owned layer safely skips only the late completion/reset operation. `+0x290/+0x291` propagation and a valid independent `+0x98` cursor may still proceed. This is a safety divergence for malformed project-owned state, not a claim about original invalid-state handling.

## Explicitly outside scope

This state layer still does not model or infer:

- the exact frame-increment/no-`+0x94` branch as one composed public operation;
- the `updateData()` call path and `+0x154` previous-frame bookkeeping;
- animation, rendering, hit effects, or action dispatch;
- resets of `+0x191/+0x290/+0x291` that are not yet proven;
- gameplay-semantic names for any offset-named state.

The host coverage now includes both the continuous combo/boundary chain and the native timing gate: top-level `+0x1bc`, one-shot `+0x168`, `delta*1000.0f` accumulation, Section-G `+0x5c` threshold composition, `+0x169` gating, threshold subtraction, invalid-index safety, and unordered floating-point behavior.

No proprietary payload or decompiler-derived source is included.
