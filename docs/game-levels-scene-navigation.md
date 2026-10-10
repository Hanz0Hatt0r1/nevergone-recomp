# GameLevels current-scene navigation

This clean-room layer joins two already recovered original boundaries: port traversal and current-scene GUID lookup.

## Evidence chain

`GameLevels::GetPortNodeLinkPortNode(EVENT_PORT_TYPE)` updates the retained current port-node pointer using the linked reverse/forward pointers for numeric event types `0` and `1`.

`GameScene::getGLGameSceneData()` at `0x0033ff12` then reads the current port node's first serialized string (`GameScenePortNodeData +0x14`) and passes it directly to `GameLevels::GetSceneDataWithGUID()`. That function scans retained scene records in array order and returns the first whose serialized scene GUID (`GameSceneData +0x20`) matches.

`GameScene::GoToCheckPointScene(EVENT_PORT_TYPE)` at `0x00343dd0` uses the port traversal function before its scene-switch/fade path, so resolving the new current scene after a successful port step is the clean-room handoff needed by a project-owned scene loader.

## Runtime boundary

`game_levels_scene_navigation::resolve_current()` maps the retained navigation index to its port GUID and then to the first matching parsed `SceneRecord`. It preserves the selected port even when its GUID is broken, distinguishing an invalid scene reference from no current port.

`step_and_resolve()` is a project-owned convenience operation: it applies the recovered port transition and immediately returns the scene selection for the resulting current port. Unsupported event types leave navigation unchanged and therefore resolve the existing current scene; a supported transition to a missing link clears both the current port and current scene.

## Next step

Use the returned `SceneRecord` index to build the first project-owned scene instance from its already parsed layers/objects and to expose the selected scene to the enter-game runtime instead of stopping at diagnostic GameLevels verification.
