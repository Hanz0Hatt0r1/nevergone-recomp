# Retained GameLevels runtime state

`game_levels_runtime_state` is the project-owned retained runtime layer above the recovered GameLevels binary framing. It loads the user-owned scene file into `game_levels_model::Model` once, keeps the current port/scene selection alive across navigation steps, and materializes the current scene into the evidence-backed construction/render representations.

## Loading

The runtime follows the reconstructed enter-game path at `<files_dir>/assets/gamescene/gs_list/pvp_scene.glData`. Files larger than 64 MiB are rejected before allocation. The model parser retains the exact verified loader end offset and does not require physical EOF equality, matching the recovered original `LoadGameLevels()` behavior.

A failed load clears any previously retained model, scene instance, construction plan and render queue so stale scene state cannot survive a failed EnterGame attempt.

`client_callback_bridge` invokes `game_levels_enter_transition::on_enter_game(files_dir)` from the real `cpp_OnEnterGame` callback. Once the probe reaches the verified PortNode section, the transition calls `load_pvp_scene()` and exposes the strongest resulting runtime boundary.

## Current scene state

A ready snapshot exposes only clean-room/project-owned values: current port index, current scene index/GUID, stored numeric event-port type, selected scene layer/object counts, construction-plan counts, and render-queue counts. The original pointer graph is never retained.

`step()` applies the recovered `GetPortNodeLinkPortNode()` semantics and immediately refreshes the current scene through the proven `current_port.first_string -> SceneRecord.string_value` mapping.

For every current scene the runtime attempts to retain:

1. `game_levels_scene_instance::SceneInstance` — the selected parsed scene expressed as project-owned values;
2. `game_scene_construction_plan::ScenePlan` — recovered layer/object traversal plus only proven object-construction semantics;
3. `game_scene_render_queue::Queue` — currently renderable object/resource requests in preserved traversal order.

The EnterGame transition consequently distinguishes `runtime-model-ready`, `runtime-scene-instance-ready`, `runtime-scene-construction-plan-ready`, and `runtime-render-queue-ready` rather than stopping at a parser-only diagnostic.

## Regression coverage

`tools/game_levels_runtime_state_smoke.cpp` builds a fully synthetic fixture, loads it through the same app-private `pvp_scene.glData` path, verifies all retained boundaries, traverses the synthetic PortNode graph, verifies stable state on an unsupported event, then exercises size-limit and missing-file failures.

The smoke also verifies that construction and render state are revoked when a later load fails. It commits no proprietary game data.

## Next boundary

P6's minimum first-scene construction boundary is present. Further progress is now downstream:

- nonzero `GameSceneObject` construction remains semantically unresolved;
- type-0 default sprite-frame objects require faithful TexturePacker plist/atlas frame resolution;
- action-pair playback, enemy/NPC creation, player spawn and gameplay state are separate later boundaries.

The runtime must not manufacture those semantics from plausible bytes. New fields should be exposed only when an original consumer or equivalent high-confidence evidence establishes their meaning.
