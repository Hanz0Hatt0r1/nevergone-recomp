# SceneActionsSystem::waUpdate(float) cursor evidence — 2026-10-12

## Native source boundary

The original ARMv7 binary exports:

- `SceneActionsSystem::waUpdate(float)` at `0x003531f1`;
- `SceneActionsSystem::waStep(float)` at `0x00353405`;
- `SceneActionsSystem::play()` at `0x00353547`;
- `SceneActionsSystem::step(float)` at `0x0035361d`.

Thumb disassembly of `waUpdate` establishes the following state flow.

## Clock

`SceneActionsSystem +0x15c` is a first-update byte. When set, `waUpdate` clears it and writes zero to the accumulator at `+0x154`; the supplied `dt` is not accumulated on that first call.

On subsequent calls the accumulator is updated by:

`accumulator += dt * 1000.0f`

The literal used by the original is IEEE-754 `0x447a0000` (`1000.0f`).

The selected primary ActionFrameData is `TrapActionsData +0x88[frame_index +0x250]`; its recovered `ActionFrameData +0x5c` value is copied to `SceneActionsSystem +0x150`. A frame advances only when `accumulator >= +0x150`, after which the threshold is subtracted from the accumulator.

## Boundary cursors

The original reads two already reconstructed TrapActions derived arrays:

- `TrapActionsData +0x94`, cursor at `SceneActionsSystem +0x25c`;
- `TrapActionsData +0x98`, cursor at `SceneActionsSystem +0x260`.

Both compare the advanced frame index against `ActionComboValue +0x18`.

For `+0x94`, cursor advancement is gated by byte `SceneActionsSystem +0x185`. `initWithFile()` initializes that byte to zero, but direct recovery of `SceneActionsSystem::play()` shows that `play()` writes `+0x185 = 1` before the first `waUpdate()`. The comparison is `frame >= field_18`. The cursor clamps at the final array element.

For `+0x98`, the comparison is strict: `frame > field_18`. Its cursor also clamps at the final array element.

If `SceneActionsSystem +0x24c` is set, any frame advancement latches byte `+0x24d` to one. `play()` resets `+0x24c` and `+0x24d` to zero when starting the recovered klhuo playback state.

`scene_actions_play_state::State` retains these control bytes explicitly together with the corrected `+0x14c` previous-frame field and the separate terminal/segment field at `+0x180`.

## Terminal transition

The exact writes after the native terminal branch has been selected are now recovered separately:

- `+0x180 = exclusive_frame_limit - 1`;
- `+0x184 = 1` (`end`);
- `+0x250 = 0` (frame cursor reset);
- `+0x1a8 = 1` (`done`) before the mode split.

Mode `0` (born) leaves `done` asserted and does not request another `updateData()`. Mode `1` (idle) clears `done` again in the same terminal path and requests `updateData()` for frame zero, closing the loop without an external restart timer.

## Project boundary

`scene_actions_wa_update::advance()` reconstructs the deterministic clock and non-terminal frame/cursor transitions. It currently reports `kTerminalBoundary` when the project-owned frame index reaches the end of the parsed primary frame vector. `scene_actions_active_playback` then applies the separately recovered terminal transition: born completes while idle loops to frame zero.

This does **not** claim that the project vector-end condition is the exact native count comparison. The original contains a signed one-past comparison described by the recovered control flow as `frame > count`, while the project parser retains exactly the serialized primary records and no sentinel. The exact mapping between that native count/index representation and the parser vector remains unresolved and must not be replaced by an invented sentinel or an asserted `count == records.size()` equivalence.

The helper also fails closed on malformed/non-positive frame thresholds and invalid cursor state instead of attempting to reproduce undefined or corrupt-input behavior.
