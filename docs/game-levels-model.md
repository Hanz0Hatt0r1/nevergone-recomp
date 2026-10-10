# Project-owned GameLevels model

The individual GameLevels parsers cover the full evidence-backed `LoadGameLevels()` stream order and the recovered post-load PortNode linking/start-scene lookup. `game_levels_model` turns those independent boundaries into one retained clean-room runtime object.

## Parse order

`game_levels_model::parse()` follows the original shared-offset loader order exactly:

1. complete `LoadGL_Scene()` section;
2. complete `LoadGL_Actions()` section using the scene format gate;
3. complete `LoadGL_Global()` section;
4. complete `LoadGL_PortNode()` section.

Each parser receives the exact preceding end offset. Failure leaves the caller's model untouched. Successful parsing records the PortNode end offset but deliberately does not require it to equal the physical file size, matching the original `LoadGameLevels()` control flow.

## Recovered post-load state

After serialized parsing succeeds, the model performs only behavior already recovered from the original binary:

- `LinkScenePortNode()` relationships are represented as project-owned graph indices;
- `GetStartScenePortNode()` selects the first marked start node, with node-zero fallback;
- the selected port's first string resolves to the first scene with the same serialized scene GUID.

The resulting model retains all four parsed sections, the graph, the startup selection and the exact verified loader end offset. It contains no original object pointers and does not reproduce unresolved gameplay-side constructors.

## Current consumers

The model is no longer diagnostic-only. `game_levels_runtime_state` retains it and uses it as the source for:

- recovered PortNode navigation;
- current-scene selection;
- `game_levels_scene_instance::SceneInstance` creation;
- `game_scene_construction_plan::ScenePlan` creation;
- the current `game_scene_render_queue::Queue`.

The construction plan deliberately interprets only semantics justified by original control flow. Ordered scene layers and their ordered objects are preserved; type `0` object construction has a proven renderer-facing subset, while nonzero object construction remains unresolved.

## Verification

`tools/game_levels_model_smoke.cpp` uses only a synthetic fixture. It verifies a successful complete four-section parse, recovered startup selection and navigation state, preservation of bytes beyond the recovered loader end offset, and transactional failure on a one-byte truncation.

`tools/game_levels_runtime_state_smoke.cpp` verifies the downstream retained runtime, scene instance, construction plan and render queue from a synthetic temporary `pvp_scene.glData`.

## Next evidence boundary

No additional serialized field is required merely to satisfy the current first-scene construction boundary. Further parser or model semantics must be driven by specific recovered consumers.

For scene construction, the first material unresolved family is nonzero `GameSceneObject` type behavior; type `6` is known to have additional pre-construction control flow tied to game/player state. For visual type `0` objects, the default `spriteFrameByName` path still depends on faithful plist/atlas resolution. These are downstream semantic/resource boundaries rather than justification for guessing more binary fields.
