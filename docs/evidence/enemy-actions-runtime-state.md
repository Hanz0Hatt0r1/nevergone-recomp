# EnemyActions composed runtime state

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This layer composes recovered `EnemyActionsSystem::comboHit()` and bounded `waUpdate()` transitions into one offset-named project-owned state object so the reconstruction can consume them as runtime behavior rather than only as isolated helpers.

## State

`enemy_actions_runtime_state::State` retains only offsets used by recovered contracts: bytes `+0x168/+0x169`, int32 `+0x154`, floats `+0x158/+0x15c/+0x160`, int32 `+0x18c`, bytes `+0x190/+0x191/+0x1bc/+0x290/+0x291/+0x292`, current frame int32 `+0x294`, and boundary indices `+0x2a0/+0x2a4`. Names remain offset-based because broader gameplay semantics are not fully proven.

## Recovered `waUpdate()` slices

`apply_wa_update_timing()` reconstructs the timing gate at `0x2ad528..0x2ad5c0`: top-level `+0x1bc`, one-shot `+0x168`, `+0x160 += delta*1000.0f`, threshold `+0x158 = ActionFrameData+0x5c + +0x15c`, `+0x169` gating, and threshold subtraction. Section G supplies the reconstructed `ActionFrameData+0x5c` value through `final_table.entries[i].reciprocal_value`.

`apply_wa_update_frame_step()` reconstructs the frame-processing path: ARM32 increment of `+0x294`, both `+0x94/+0x98` boundary streams, the no-`+0x94` fallback at `0x2ad6b0..0x2ad716`, count-based reset, and `+0x154`/`updateData()` call decision at `0x2ad71a..0x2ad738`. The helper reports `should_call_update_data`; it does not implement `updateData()` internals.

`apply_wa_update_iteration()` connects a parsed Sections A-G `Document` directly to the runtime state for exactly one timing→frame-step iteration. It intentionally does not reproduce the native back-edge that may process multiple accumulated frames in one call.

## `showActionLastFrame()`

The native `EnemyActionsSystem::showActionLastFrame()` entry at `0x2ad748` is a small independent state transition. The function loads int32 `system+0x18c`, stores it directly to current frame `system+0x294`, then transfers to `EnemyActionsSystem::updateData()` unconditionally.

`apply_show_action_last_frame()` mirrors exactly the proven state-visible portion:

- `current_frame_294 = field_18c` with no normalization or range check;
- all other reconstructed state, including previous-frame storage `+0x154`, is preserved before the downstream call;
- `should_call_update_data` is always true.

The helper surfaces the call as a signal rather than assigning behavior to `updateData()` that has not yet been reconstructed.

## Explicitly outside scope

This state layer still does not model the internal behavior of `EnemyActionsSystem::updateData()`, animation/render/hit-effect dispatch, unproven resets of `+0x191/+0x290/+0x291`, semantic names for offset fields, or the full native multi-frame back-edge policy.

Host coverage spans the combo/boundary chain, timing gate, both frame-processing branches, ARM32 frame increment, no-`+0x94` fallback, count-based reset, `+0x154` bookkeeping, parsed-document single iteration, and `showActionLastFrame()`.

No proprietary payload or decompiler-derived source is included.
