# Project-owned GameLevels model

The individual GameLevels parsers now cover the full evidence-backed `LoadGameLevels()` stream order and the recovered post-load PortNode linking/start-scene lookup. `game_levels_model` turns those independent boundaries into one retained clean-room runtime object.

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

## Next step

Use `Model` as the input to project-owned scene navigation and construction. The next bounded behavior is `GetPortNodeLinkPortNode(EVENT_PORT_TYPE)`, after which the selected `SceneRecord` can be handed to a minimal first-scene runtime instead of being reparsed or rediscovered through probe-only state.
