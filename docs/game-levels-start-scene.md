# GameLevels start-scene lookup evidence

This note records the clean-room startup lookup from Never Gone 1.0.9 ARMv7 after `LoadGameLevels()` finishes.

## Start port selection

`GameLevels::GetStartScenePortNode()` is at disassembly address `0x002c24f4`. It scans the retained port-node array in order and reads the byte at object offset `+0x4a8`. `LoadGL_PortNode()` writes the first serialized bool to that exact field, proving `PortNodeRecord::first_bool` is the start-node marker used by this lookup.

The first node with a true marker becomes the current port node and the stored event-port type is reset to zero. If no node is marked but the array is non-empty, node zero is selected and the event-port type is also reset. If the array is empty, the original returns the existing current-node pointer.

## Port to scene GUID

`LoadGL_Scene()` stores each scene record's first serialized string in `GameSceneData +0x20` at `0x002c28c4`. `GameLevels::GetSceneDataWithGUID()` at `0x002c2380` iterates scene records and compares that `+0x20` string, returning the first match.

`GameScene::getGLGameSceneData()` at `0x0033ff12` reads the current port node's first serialized string (`GameScenePortNodeData +0x14`) and passes it directly to `GetSceneDataWithGUID()`. This proves the startup mapping:

`selected PortNodeRecord::first_string -> SceneRecord::string_value`

`GameScene::LoadGameLevelsWithFile()` at `0x0033fec0` calls `LoadGameLevels()` and then `GetStartScenePortNode()`, confirming start-node selection immediately follows level loading in the original flow.

## Clean-room resolver

`game_levels_start_scene::resolve()` preserves the first-marked-node rule, index-zero fallback, and first matching scene GUID. It exposes a selected port even when no matching scene exists, allowing the runtime to distinguish broken scene references from an empty port-node table.

## Next step

Use the resolved scene record together with the reconstructed port-node graph to implement evidence-backed port traversal (`GetPortNodeLinkPortNode`) and then feed the selected scene into the project-owned first-scene construction path.
