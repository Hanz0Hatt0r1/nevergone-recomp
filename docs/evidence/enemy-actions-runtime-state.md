# EnemyActions composed runtime state

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This layer does not add new native evidence. It composes the already recovered `EnemyActionsSystem::comboHit()` and bounded `waUpdate()` transitions into one offset-named project-owned state object so the reconstruction can consume them as runtime behavior rather than only as isolated helpers.

## State

`enemy_actions_runtime_state::State` retains only offsets already used by the recovered contracts:

- byte `+0x169`;
- byte `+0x190`;
- byte `+0x191`;
- byte `+0x1bc`;
- byte `+0x290`;
- byte `+0x291`;
- byte `+0x292`;
- int32 `+0x18c`;
- current frame int32 `+0x294`;
- `+0x94` boundary index `+0x2a0`;
- `+0x98` boundary index `+0x2a4`.

Names remain offset-based because gameplay semantics for these fields are not fully proven.

## `apply_combo_hit()`

The runtime wrapper supplies `State.boundary_index_2a0` and `State.current_frame_294` to the recovered `comboHit()` transition and maps the proven `+0x190/+0x191/+0x290` writes back into the same state. All unrelated fields are preserved.

Thus the runtime state directly preserves the already recovered native gates and inclusive `ActionComboValue+0x14..+0x18` range test without duplicating them.

## `apply_wa_update_after_frame_advance()`

The caller must supply `State.current_frame_294` **after** whatever native-equivalent timing/frame-advance logic is reconstructed elsewhere. This function deliberately does not increment the frame and does not infer the unresolved `ActionFrameData+0x5c` timing behavior.

The wrapper then:

1. captures the selected pre-transition `+0x94` object's `field_18` endpoint when the current `+0x2a0` index is valid;
2. applies the recovered `+0x290 -> +0x291` propagation;
3. applies the `+0x191`-gated `+0x94/+0x2a0` boundary update;
4. applies the independent `+0x98/+0x2a4` boundary update;
5. when the pre-transition `+0x94` endpoint was valid, applies the recovered strict post-endpoint completion/reset transition using that captured endpoint and the real-advance result from the `+0x94` cursor.

If reconstructed `+0x94` state is invalid, the project-owned layer safely skips only the late completion/reset operation. `+0x290/+0x291` propagation and a valid independent `+0x98` cursor may still proceed. This is a safety divergence for malformed project-owned state, not a claim about original invalid-state handling.

## Explicitly outside scope

This state layer does not model or infer:

- the frame increment itself;
- `ActionFrameData+0x5c` timing math;
- the `updateData()` call path;
- animation, rendering, hit effects, or action dispatch;
- resets of `+0x191/+0x290/+0x291` that are not yet proven;
- gameplay-semantic names for any offset-named state.

The host smoke exercises a continuous recovered chain: a mode-2 `comboHit`, byte propagation and dual boundary handling in `waUpdate`, final-boundary post-endpoint reset, gating of a subsequent hit by `+0x190`, suppression of the reset on a real +0x94 advance, and safe isolation of an invalid +0x94 cursor while +0x98 remains valid.
