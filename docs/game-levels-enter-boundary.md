# GameLevels enter-game boundary

This document records the current clean-room `cpp_OnEnterGame -> GameLevels` boundary. Earlier revisions stopped at the first-object core; the project has since advanced through the full evidence-backed `LoadGameLevels()` stream and into a retained scene-construction model.

## Trigger

The login callback router maps the real `cpp_OnEnterGame` callback to `ManagementRoute::kEnteringGame`. After callback-state locking is released, `game_levels_enter_transition::on_enter_game()` probes and then loads the user-imported resource:

`<files>/assets/gamescene/gs_list/pvp_scene.glData`

No original `libcocos2dcpp.so` object graph is required for this handoff.

## Current verified loader boundary

The bounded parser now covers the evidence-backed `LoadGameLevels()` order:

1. complete `LoadGL_Scene()` section;
2. complete `LoadGL_Actions()` section using the recovered scene-format gate;
3. complete `LoadGL_Global()` section;
4. complete `LoadGL_PortNode()` section;
5. recovered PortNode linking and start-node/start-scene selection.

Every section consumes the exact end offset of the previous one. `game_levels_model::parse()` is transactional: if any required section is truncated or invalid, the caller's model is left unchanged. A successful parse retains the verified loader end offset without requiring physical EOF equality, matching the recovered original control flow.

The earlier first-object prefix/core parsers remain useful evidence milestones, but they are no longer the strongest runtime boundary.

## P6 scene-construction boundary

A valid model is retained by `game_levels_runtime_state`, which resolves the selected scene from the recovered PortNode mapping and builds:

- a project-owned `game_levels_scene_instance::SceneInstance`;
- a project-owned `game_scene_construction_plan::ScenePlan` preserving recovered layer/object traversal order;
- a `game_scene_render_queue::Queue` for the currently supported construction classes.

`GameScene::loadingTex()` evidence establishes ordered layer slots `0..10`. Within those layers, the serialized leading object integer is proven to be the object type code. Type `0` has a recovered construction family: default sprite-frame-cache lookup, several exact-name direct-file branches, the `klhuo-1.png` SceneActions pair branch, and the common sprite transform mapping for position, rotation, scale, flip-X and local child Z.

This is the minimum evidence-backed data needed to construct the first project-owned offline scene representation. It does not claim that all GameScene object classes, actions, enemies, physics or player spawning are reconstructed.

## Transactional and synthetic regression coverage

`tools/game_levels_model_smoke.cpp` constructs a synthetic complete scene/actions/global/port-node stream, verifies the retained model and start-scene selection, truncates the stream, and verifies both failure and unchanged caller state.

`tools/game_levels_runtime_state_smoke.cpp` writes a synthetic `pvp_scene.glData` under a temporary imported-data tree and verifies retained model state, scene instance creation, construction-plan creation, render-queue creation, navigation, unsupported transitions, size rejection and missing-file failure. No proprietary scene bytes are committed by either regression.

## Exact next unresolved boundary

The parser-side four-section stream needed for the current P6 construction model is complete at the evidence-backed boundary. The next unresolved scene-construction semantics are not another guessed byte field:

- nonzero `GameSceneObject` type codes do not yet have equivalent project-owned construction semantics;
- type `6` has known original pre-construction handling, but it depends on additional game-mode/player state and is intentionally not modeled yet;
- for type `0`, the second serialized boolean and later conditional/tail fields remain structurally parsed but are not assigned unsupported rendering semantics;
- the default type-0 `spriteFrameByName` path still requires faithful TexturePacker plist/atlas resolution before those objects can join the direct-file sprites in the visual renderer.

The renderer and later gameplay work must advance from those evidence boundaries rather than extending the binary parser speculatively. See `game-scene-object-construction.md` for the recovered object-construction details.
