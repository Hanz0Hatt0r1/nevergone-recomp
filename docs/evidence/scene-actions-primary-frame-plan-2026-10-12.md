# SceneActions primary-frame renderer plan — 2026-10-12

The current `SceneActionsSystem::updateData()` primary sprite behavior is now split into two independently recovered geometry helpers:

- `scene_actions_base_sprite_update::build_non_flipped()` for `+0x15e == 0`;
- `scene_actions_flip_x_geometry::build()` for the distinct reflected branch.

`scene_actions_primary_frame_plan::build()` composes those branches at the selected TrapActions primary `frame_index` and retains the current AFD resource request (`+0x60` atlas plist and `+0x68` sprite-frame name) together with one renderer-facing geometry record.

The plan contains:

- source primary frame index;
- atlas plist and frame name;
- position;
- anchor point;
- rotation;
- scale X/Y;
- final flip-X state.

The helper fails closed if the frame index is outside the parsed primary array, if the resource strings are absent, or if the selected geometry branch rejects malformed inputs.

This is intentionally a pure descriptor. It does not decode or upload pixels, mutate GL state, advance the `waUpdate` cursor, or implement the secondary SceneActions sprite path. The next Android/runtime boundary can therefore consume one current-frame descriptor without having to duplicate native geometry selection.
