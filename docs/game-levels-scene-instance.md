# Project-owned GameLevels scene instance

The recovered GameLevels model now identifies a concrete current `SceneRecord`, but the original `GameScene::createGSObject()` constructor remains a large unresolved gameplay boundary. `game_levels_scene_instance` deliberately bridges that gap without inventing original object semantics.

## Structural instance

`SceneInstance` copies only data already proven by the serialized `LoadGL_Scene()` reconstruction:

- source scene index and scene GUID;
- the two recovered scene point values;
- ordered layers and each layer's recovered float;
- complete parsed `ObjectRecord` values in original order;
- recovered top/bottom border point lists;
- a derived total object count.

The copy contains no original pointers and is independent from later mutations of the retained GameLevels model. This makes it suitable as a stable input to future project-owned rendering/physics adapters while keeping unresolved gameplay constructors outside the trusted boundary.

`build_current()` uses the already recovered current-port GUID lookup and returns no instance when there is no current port or when its scene GUID cannot be resolved.

## Original handoff evidence

`GameScene::loadGameSceneData(GameSceneData*)` at `0x00346c44` stores the selected scene data before the original texture/object loading path. `GameScene::gsScene(unsigned int)` at `0x00346c72` resolves the current port to scene data and calls that loader. `GameScene::createGSObject()` at `0x00343d6c` remains intentionally outside this reconstruction until its object-type semantics are proven.

## Next step

Expose the current structural `SceneInstance` from `game_levels_runtime_state`, then build the first minimal renderer from proven scene/layer/object fields rather than recreating unresolved Cocos object constructors wholesale.
