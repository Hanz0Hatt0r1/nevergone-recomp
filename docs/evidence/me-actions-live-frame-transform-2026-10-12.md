# MeActions live primary-frame transform — 2026-10-12

## Clean-room scope

This note records bounded behavior recovered from the shipped ARMv7 `libcocos2dcpp.so`. No original binary, proprietary action payload, screenshot, or disassembly listing is committed.

## Current-frame identity

`MeActionsSystem::updateData()` begins its primary body path by indexing the action-data primary frame array with `MeActionsSystem +0x1a4`. The same selected `ActionFrameData` supplies the sprite-frame name at `+0x68` and the body transform fields used during that call.

The relevant native region is the already identified `MeActionsSystem::updateData()` function at ELF `0x00329d34`. The primary body update around `0x00329ee4..0x0032a128` applies, from the currently selected frame:

- position inputs from `AFD +0x14/+0x18`;
- anchor from `AFD +0x24/+0x28`;
- scale inputs from `AFD +0x44/+0x48`;
- rotation from `AFD +0x34`;
- frame flip from `AFD +0x40`;
- child Z from `AFD +0x38`;
- display frame from `AFD +0x68`.

This proves that later primary frames do not retain frame-zero transform values merely because the sprite-frame key advanced.

## Supported play path

The overload `MeActionsSystem::play(int,float)` at ELF `0x0032b7c6` initializes a new playback and explicitly stores zero into `MeActionsSystem +0x204` and `+0x208` before selecting the start frame and calling `updateData()`.

The project-owned live action route models this zero-offset playback path and does not currently expose non-zero action-local offset parameters. Therefore the existing validated `ActionFrameData` transform reconstruction can be applied to the currently selected frame rather than only `primary_records.front()`.

## General native formula boundary

Static inspection also confirms that the full native position/scale path can depend on additional mutable system state, including:

- `MeActionsSystem +0x204/+0x208` action-local position offsets;
- `+0x2f8` scale multiplier/divisor;
- `+0x644/+0x648` cut/H-point state;
- `+0x1be` facing inversion;
- attachment/movement state updated elsewhere.

Those general-state paths are not inferred here. The reconstruction remains bounded to the currently supported zero-offset player playback and the independently recovered live facing state.

## Project change

`game_scene_player_render_queue::with_live_player()` now builds `player_body_transform` from the same selected primary record identified by `current_frame_1a4`. Regression coverage gives frame 1 transform values different from frame 0 and requires the renderer-facing command to carry frame 1 position, anchor, scale, rotation, child Z, atlas plist and sprite-frame key together.
