# MeActions player playback timing — 2026-10-12

## Clean-room boundary

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records bounded behavior from exported `MeActionsSystem` functions. No original binary, disassembly listing, action payload, image, or screenshot is committed.

The player playback object is not treated as an `EnemyActionsSystem`: the action-frame payload shape is compatible, but player runtime fields and gates are at different offsets.

## `isDone()`

Exported `MeActionsSystem::isDone()` starts at ELF `0x003292d0` and consists of one state read: it returns byte `MeActionsSystem + 0x220`.

Therefore the project can represent player action completion exactly as `done_220` without inventing a duration or deriving completion from a button release.

## `play()` initialization

`MeActionsSystem::play()` starts at ELF `0x0032b5d8`. Its initialization establishes the playback state used by `waUpdate()`:

- `+0x220 = 0` — not done;
- `+0x1bc = 1` — reset accumulator on the next update;
- `+0x1a8 = -1` — previous frame;
- `+0x1a4 = 0` — current frame;
- `+0x1f8 = 0`, `+0x1fc = 0` — boundary-array indices;
- `+0x1b4 = 0` — timing accumulator;
- `+0x1b0 = 0` — attach-action duration contribution.

After selecting primary frame zero, `play()` copies that `ActionFrameData + 0x5c` value into `MeActionsSystem + 0x1ac` and calls `updateData()`.

## `waUpdate(float)` timing prefix

`MeActionsSystem::waUpdate(float)` starts at ELF `0x0032b38c`.

Before any `+0xb0/+0xb4` action-data boundary processing, the native function performs this deterministic player-specific prefix:

1. if byte `+0x220` is nonzero, return immediately;
2. if byte `+0x1bc` is nonzero, clear it and store zero to `+0x1b4`; the input delta is not accumulated on that update;
3. otherwise accumulate `delta_seconds * 1000.0f` into `+0x1b4`;
4. select the current primary `ActionFrameData` through action data `+0x34` and current frame `+0x1a4`;
5. calculate `+0x1ac = ActionFrameData+0x5c + system+0x1b0`;
6. if accumulator `+0x1b4` is below that threshold, return;
7. otherwise subtract the threshold from `+0x1b4` and continue into frame/boundary processing.

The `1000.0f` multiplier is the native literal `0x447a0000`.

The existing Section-G parser already proves the project-owned source for `ActionFrameData+0x5c`: `document.final_table.entries[frame].reciprocal_value`. The playback timing contract therefore consumes that parsed value directly.

The native comparison is `VCMPE` followed by `BLT`; unordered/NaN comparison takes the early-return path. The project helper preserves that behavior explicitly.

## Frame advancement and completion boundary

The same `waUpdate()` function later increments player current frame `+0x1a4`, updates `+0x1f8/+0x1fc` against action-data arrays `+0xb0/+0xb4`, and sets `+0x220 = 1` on its recovered completion/reset paths. It also compares `+0x1a8` with `+0x1a4` and calls `updateData()` only when the frame changed and the native update-data gate is enabled.

Those later branches are intentionally not folded into `me_actions_playback_timing` yet. The exact serialized-document mapping for the `+0xb0/+0xb4` arrays must be retained before enabling live frame advancement. Until then the runtime must not guess a frame count or force return-to-idle from button release.

## Project contract

`me_actions_playback_timing` now provides:

- exact `play()` timing initialization for `+0x1b0/+0x1b4/+0x1bc/+0x220`;
- exact `isDone()` semantics through `done_220`;
- exact `waUpdate(float)` timing accumulation/threshold/subtraction up to the frame-processing branch;
- project-owned bounds checks for malformed current-frame state.

This closes the player timing gate while keeping action-boundary advancement as the next evidence-backed reconstruction step.
