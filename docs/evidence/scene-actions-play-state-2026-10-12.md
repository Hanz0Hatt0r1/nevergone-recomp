# SceneActions play initialization — 2026-10-12

## Scope

This note records the state initialized by `SceneActionsSystem::play()` before `updateData()` for the recovered klhuo born/idle pair.

## Recovered state

When `SceneActionsSystem +0x148` contains loaded TrapActionsData, `play()` initializes the playback state with:

- active byte `+0x15d = 1`;
- first-update byte `+0x15c = 1`;
- frame index `+0x250 = 0`;
- TrapActions `+0x94` boundary cursor `+0x25c = 0`;
- TrapActions `+0x98` boundary cursor `+0x260 = 0`;
- accumulator `+0x154 = 0`;
- done byte `+0x1a8 = 0`;
- end byte `+0x184 = 0`;
- last-frame field `+0x14c = -1`;
- secondary last/index field `+0x170 = -1`.

The current primary ActionFrameData is selected at index 0. Its already reconstructed `+0x5c` value is copied to `SceneActionsSystem +0x150` before `updateData()`.

`TrapActionsData::loadWBGFile()` derives each primary ActionFrameData `+0x5c` from the final per-primary serialized integer table as `1.0f / float(serialized_value)`. The project therefore sources the initial `+0x150` value from `trap_actions_document::Document.final_table.entries[0].reciprocal_value` rather than fabricating a duration.

## Project boundary

`scene_actions_play_state::build()` reconstructs only this deterministic initialization state for proven klhuo modes 0 and 1. It verifies that a primary frame and matching final-table entry exist and rejects non-finite values.

It deliberately leaves the `+0x150` member structurally named `current_frame_value_150`; interpreting and advancing it belongs to the separate `waUpdate(float)` reconstruction. Other zeroed native fields that are not yet consumed by project playback are not assigned invented semantics here.
