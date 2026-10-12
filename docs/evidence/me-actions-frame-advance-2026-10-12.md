# MeActions player frame advancement — 2026-10-12

## Clean-room boundary

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note extends the already recovered `MeActionsSystem::waUpdate(float)` timing prefix through its player frame/boundary loop. No original binary, disassembly listing, action payload, image or screenshot is committed.

## Inputs already recovered

The project already has two evidence-backed inputs needed by this branch:

- Section-G `ActionFrameData +0x5c` durations through `document.final_table.entries[].reciprocal_value`;
- Section-F player boundary arrays through `me_actions_combo_boundaries::derive()`: `MeActionsData +0xb0` from tuple int32[3]+1, and `+0xb4` from tuple int32[1]+1.

Each derived `ActionComboValue` stores its source tuple/frame boundary at object `+0x18`.

## Primary boundary path

After the timing threshold is consumed, native `waUpdate()` increments current frame `+0x1a4`. If `+0xb0` is non-empty, the current `+0xb0` entry supplies an inclusive frame boundary through `ActionComboValue +0x18`.

When `+0x201` is enabled and the incremented current frame is greater than or equal to that boundary, native increments boundary cursor `+0x1f8`. If another boundary entry exists, playback continues into the next run. If the cursor reaches the end, native clamps it back to the last entry rather than storing one-past-end.

When no next primary run exists, playback continues while current frame is less than or equal to the final primary boundary. Once current frame is greater than the final boundary, native:

- sets byte `+0x200`;
- sets done byte `+0x220`;
- resets current frame `+0x1a4` to zero;
- stores `primary_boundary - 1` at `+0x1f4`.

## Secondary boundary cursor

The `+0xb4` array is indexed by `+0x1fc` and is independently clamped to its final entry.

In the primary-boundary branch, `+0x1fc` advances when incremented current frame is **greater than or equal to** the current `+0xb4` boundary.

In the fallback branch where `+0xb0` is empty, native advances `+0x1fc` only when current frame is **strictly greater than** the `+0xb4` boundary. The reconstructed helper preserves this `>=` versus `>` difference.

## Fallback completion

If `+0xb0` is empty, native uses the primary ActionFrameData array count as its completion boundary. On completion it resets current frame to zero, marks `+0x200/+0x220`, and stores `count - 1` in `+0x1f4`.

## Same-call iteration

After frame processing native compares previous frame `+0x1a8` with current `+0x1a4`. If they differ, it conditionally calls `updateData()` through byte gate `+0x1bf`, copies current frame into previous frame, and jumps back to the timing comparison.

It does not reapply the caller's `delta` during those internal iterations. Any additional frames in the same `waUpdate()` call are driven only by the retained accumulator left after threshold subtraction.

`apply_wa_update_playback()` mirrors this behavior: supplied `delta_seconds` is accumulated once, and subsequent same-call iterations use zero new delta while consuming the retained millisecond accumulator.

## Project boundary

`me_actions_playback_timing::Cursor` now mirrors the recovered player frame cursor, previous-frame cursor, `+0x1f8/+0x1fc` boundary cursors, last-frame field and completion/update gates. `apply_wa_update_playback()` exposes frame advancement and the exact `done_220` transition.

The next runtime integration is to retain this cursor for the currently loaded player action and feed changing `current_frame_1a4` values into the existing player frame-request/texture staging path. That will provide live multi-frame playback for jump/A/B actions and the first evidence-backed completion signal for return-to-idle logic.
