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

Each entry becomes a `GameLevelsData` object. In the recovered assignment sequence, the objects read from `gsresfile01`, `gsresfile02`, `gsresfile03`, and `gsresfile04` are retained at `GameLevelsData + 0x18`, `+0x1c`, `+0x20`, and `+0x24` respectively. The loader does not convert those four values through `CCString`; it retains the plist objects directly. The downstream `loadingTex()` use below proves the `+0x24`/`gsresfile04` object is a `CCArray` of strings.

## Atlas preload list used by `loadingTex()`

At the beginning of `GameScene::loadingTex()` (`0x00346678`), the original follows the current `GameLevelsData` pointer from `GameScene + 0x238`, reads `GameLevelsData + 0x24`, and treats it as a `CCArray`.

For every element in that array, in original order, it performs:

1. `CCArray::objectAtIndex(index)`;
2. `CCString::getCString()`;
3. `CCSpriteFrameCache::sharedSpriteFrameCache()`;
4. `CCSpriteFrameCache::addSpriteFramesWithFile(plist_name)`;
5. records the same string object in the GameScene-owned loaded-resource array at `GameScene + 0x24c`.

Only after this preload loop does `loadingTex()` create the 11 scene layers and walk scene objects. This explains why the default type-0 object path can later call `spriteFrameByName(serialized_name)` without knowing an atlas file locally.

The clean-room renderer therefore preserves this separation:

- **direct-file sprite:** resolve the proven imported image search path immediately;
- **sprite-frame-by-name:** resolve only after the recovered `gsresfile04` plist list for the selected scene-list entry has been loaded;
- **scene-action pair:** defer to the separate action-system reconstruction.

## Scene-list filename boundary

A complete Thumb-2 scan of the shipped `.text` section finds no direct `BL` or `B.W` branch to `GameScene::loadGameSceneList(char const*)`. The symbol is exported, but the library contains no direct in-library call edge from which a constant plist filename can be recovered. A simple string-table scan likewise exposes the schema keys and GameScene search paths but not a unique scene-list plist filename.

That means hardcoding a guessed filename would not be clean-room evidence. The project-owned runtime instead uses `GameSceneAtlasListDiscovery` to identify the imported source by content:

- search only beneath the user-owned `filesDir/assets/gamescene` tree;
- inspect bounded `.plist` candidates only;
- require the recovered top-level `gs_num` plus `gs%02d` dictionary schema;
- match the target entry by the recovered `gamedatafile` key, using `pvp_scene.glData` as the currently reconstructed GameLevels data file;
- extract the `gsresfile04` array in original order;
- reject traversal resource names, oversized inputs, excessive entry counts, and ambiguous multiple matching scene-list files.

This resolves the imported atlas preload list without assigning an unproven filename or redistributing any game resources.

## Sprite-frame atlas texture resolution

`cocos2d::CCSpriteFrameCache::addSpriteFramesWithFile(char const*)` starts at aligned Thumb address `0x0053bccc`. After `CCFileUtils` resolves and loads the plist dictionary, the original reads the top-level `metadata` dictionary and asks it directly for the key `textureFileName`.

When direct `metadata.textureFileName` is non-empty, the function resolves that texture name against the resolved plist path through `CCFileUtils` before passing the resulting image path to `CCTextureCache::addImage()`. When the direct metadata key is absent or empty, it derives the image name by replacing the plist extension with `.png` and loads that sibling image instead.

This direct lookup is significant for TexturePacker variants that also contain a nested `metadata/target/textureFileName`: the recovered loader does not consult that nested target value at this boundary. If the direct key is absent, the sibling `.png` fallback remains the original behavior.

`GameSceneAtlasResolver` reproduces only those proven rules for user-imported assets:

- resolve each ordered `gsresfile04` plist beneath the recovered GameScene search roots;
- require a unique existing plist match rather than inventing precedence when multiple imported files match one resource name;
- honor direct `metadata.textureFileName` relative to the plist directory;
- otherwise use the same plist basename with a `.png` extension;
- require the resulting texture file to remain under the imported asset root and to exist;
- reject traversal and malformed resource paths.

With this boundary, the next staging step can combine the live `spriteFrameByName` request snapshot with the ordered atlas list, locate the requested frame metadata, decode only user-owned atlas pixels, and retain the frame's TexturePacker source-size/offset information for rendering.
