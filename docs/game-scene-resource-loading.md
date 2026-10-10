# GameScene resource-loading evidence

This note records clean-room resource-loading behavior recovered from the user's original Never Gone 1.0.9 ARMv7 library. It documents only interfaces needed by the project-owned runtime and does not contain proprietary assets.

## Cocos search path used by direct sprites

`AppDelegate::applicationDidFinishLaunching()` starts at `0x00291ac0` and calls `AppDelegate::AddAllSearchPath()` at `0x002915d8` before the first scene is run.

`AddAllSearchPath()` builds the resource search-path vector and invokes the `CCFileUtils` virtual slot at object-vtable offset `+0x34`. The shipped `CCFileUtils` vtable at `0x00925e70` resolves that slot to `CCFileUtils::setSearchPaths(...)` at `0x0053334c` (Thumb symbol value `0x0053334d`). The recovered path strings include:

- `gamescene`
- `gamescene/gs_actions/res`
- `gamescene/task`
- `gamescene/gs_list`
- `gamescene/scene_data_files`
- `gamescene/gs_res_image_file`

The already recovered direct type-0 branches pass basenames such as `gktianchong.png` and `gkyuanjing.png` to `CCSprite::create(char const*)`. Therefore the clean-room imported path for those direct sprite resources is evidence-backed as:

`filesDir/assets/gamescene/gs_res_image_file/<basename>`

`game_scene_direct_asset` exposes only the `filesDir/assets`-relative suffix and rejects path separators/traversal components before a Java/native loader can open the user-owned file.

## Scene-list entry fields

`GameScene::loadGameSceneList(char const*)` starts at `0x00341118`. It loads a plist dictionary and reads these exact keys from the shipped string table:

- `gs_num`
- per-entry key format `gs%02d`
- `gamedatafile`
- `eventID`
- `minMoveH`
- `maxMoveH`
- `gsresfile01`
- `gsresfile02`
- `gsresfile03`
- `gsresfile04`
- `playerStartXPos`
- `playerStartYPos`
- `sceneNum`
- `mirror`

Each entry becomes a `GameLevelsData` object. In the recovered assignment sequence, the object read from `gsresfile04` is retained at `GameLevelsData + 0x24`.

## Atlas preload list used by `loadingTex()`

At the beginning of `GameScene::loadingTex()` (`0x00346678`), the original follows the current `GameLevelsData` pointer from `GameScene + 0x238`, reads `GameLevelsData + 0x24`, and treats it as a `CCArray`.

For every element in that array, in original order, it performs:

1. `CCArray::objectAtIndex(index)`;
2. `CCString::getCString()`;
3. `CCSpriteFrameCache::sharedSpriteFrameCache()`;
4. `CCSpriteFrameCache::addSpriteFramesWithFile(plist_name)`;
5. records the same string object in the GameScene-owned loaded-resource array at `GameScene + 0x24c`.

Only after this preload loop does `loadingTex()` create the 11 scene layers and walk scene objects. This explains why the default type-0 object path can later call `spriteFrameByName(serialized_name)` without knowing an atlas file locally.

The clean-room renderer should therefore preserve this separation:

- **direct-file sprite:** resolve the proven imported image search path immediately;
- **sprite-frame-by-name:** resolve only after the recovered `gsresfile04` plist list for the selected scene-list entry has been loaded;
- **scene-action pair:** defer to the separate action-system reconstruction.

The exact scene-list plist filename supplied to `loadGameSceneList()` is not yet recovered, so the project must not invent one. Atlas registration should remain a distinct milestone until that source path (or an equivalent user-owned imported entry point) is proven.
