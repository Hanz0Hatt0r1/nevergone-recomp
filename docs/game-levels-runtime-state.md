# Retained GameLevels runtime state

`game_levels_runtime_state` is the first project-owned retained runtime layer above the recovered GameLevels binary framing. It loads the user-owned scene file into `game_levels_model::Model` once and keeps the current port/scene selection alive across navigation steps.

## Loading

The runtime currently follows the same reconstructed offline path already used by the enter-game probe: `<files_dir>/assets/gamescene/gs_list/pvp_scene.glData`. Files larger than 64 MiB are rejected before allocation. The model parser retains the exact verified loader end offset and does not require physical EOF equality, matching the recovered original `LoadGameLevels()` behavior.

A failed load clears any previously retained model, preventing stale scene state from surviving a failed EnterGame attempt.

## Current scene state

A ready snapshot exposes only clean-room/project-owned values: current port index, current scene index/GUID, stored numeric event-port type, selected scene layer count and total parsed object count. The original pointer graph is never retained.

`step()` applies the recovered `GetPortNodeLinkPortNode()` semantics and immediately refreshes current scene selection through the proven `current_port.first_string -> SceneRecord.string_value` mapping.

## Next step

Invoke `load_pvp_scene()` from the actual EnterGame callback path after the existing verification probe. Once that is live, the selected `SceneRecord` can be handed to a project-owned scene instance/renderer rather than stopping at diagnostics.
