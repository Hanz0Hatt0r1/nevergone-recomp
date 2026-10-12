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

For `+0x94`, cursor advancement is gated by byte `SceneActionsSystem +0x185`, which `comboHit()` sets after the current frame lies inside the selected combo range. The comparison is `frame >= field_18`. The cursor clamps at the final array element.

For `+0x98`, the comparison is strict: `frame > field_18`. Its cursor also clamps at the final array element.

If `SceneActionsSystem +0x24c` is set, any frame advancement latches byte `+0x24d` to one.

`initWithFile()` initializes `+0x185`, `+0x24c`, and `+0x24d` to zero. `scene_actions_play_state::State` now retains these three bytes explicitly.

## Project boundary

`scene_actions_wa_update::advance()` reconstructs the deterministic clock and the non-terminal frame/cursor transitions. It deliberately reports a separate `kTerminalBoundary` when the project-owned frame index reaches the end of the parsed primary frame vector.

That separation is intentional: the native function contains a later signed one-past-frame completion branch using `frame > count`, plus mode-specific done/end writes already partially recovered in the separate completion-mode work. This increment does not silently replace that native edge with a conventional `frame >= size` rule.

The helper also fails closed on malformed/non-positive frame thresholds and invalid cursor state instead of attempting to reproduce undefined or corrupt-input behavior.
