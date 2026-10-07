# Resource search path reconstruction

`AppDelegate::AddAllSearchPath()` has been reconstructed far enough to recover the complete directory list passed to Cocos2d-x.

## Root path

The function calls `CCFileUtils::sharedFileUtils()` and dispatches virtual slot `+0x64` on the Android implementation. The `CCFileUtilsAndroid` vtable resolves that slot to:

```text
cocos2d::CCFileUtilsAndroid::getWritablePath()
```

`getWritablePath()` calls the Java-side file-directory helper and, when the returned directory is non-empty, appends `/`.

`AddAllSearchPath()` then appends the literal:

```text
assets
```

Therefore the root used by this function is conceptually:

```text
<Android app files directory>/assets
```

The exact absolute prefix is supplied at runtime by the Java helper and should not be hard-coded in the recompilation.

## Search-path installation

The function first inserts the root path itself, then constructs 59 child paths with:

```text
%s/%s
```

where the first `%s` is the writable `assets` root and the second is one of the directory names below.

After the vector is complete, virtual slot `+0x34` on `CCFileUtils` is called. The vtable resolves this slot to:

```text
cocos2d::CCFileUtils::setSearchPaths(
    std::vector<std::string, std::allocator<std::string>> const&)
```

So the recovered behavior is approximately:

```cpp
void AppDelegate::AddAllSearchPath() {
    auto* files = cocos2d::CCFileUtils::sharedFileUtils();
    std::string root = files->getWritablePath();
    root += "assets";

    std::vector<std::string> paths;
    paths.emplace_back(root);

    for (const char* directory : kGameSearchDirectories) {
        paths.emplace_back(CCString::createWithFormat(
            "%s/%s", root.c_str(), directory)->getCString());
    }

    files->setSearchPaths(paths);
}
```

The original loop terminates when its index reaches `0x3b`, confirming exactly **59 child directories** in addition to the root.

## Recovered child directories

The order below follows the original stack array / loop order:

```text
player
player/Player01_Res
player/Player01_Res/Weapons_Res
player/Player01_Res/bz_res
player/Player02_Res
player/Player02_Res/Weapons_Res
player/Player02_Res/bz_res
player/Player03_Res
player/Player03_Res/Weapons_Res
player/actions_data/adata/p01/normal
player/actions_data/adata/p02/normal
player/actions_data/adata/p03/normal
player/actions_data/T_adata/normal
other
sound
sound/ENCV
sound/UI_Sound
sound/player01_sound
sound/player02_sound
sound/player03_sound
gamescene
gamescene/gs_actions/res
gamescene/task
gamescene/gs_list
gamescene/scene_data_files
gamescene/gs_res_image_file
shader
controls_ui
enemy
enemy/enemy_t_res
enemy/eo_files
enemy/enemy_ai
pets
pets/pets_t_res
pets/pets_ai
lansquenet
lansquenet/l_t_res
lansquenet/lansquenet_ai
NPC
NPC/npc_e_res
weapons
equips
Login/ChooseHero
Login/LoginScreenUI
NewMap
NewMap/CL_UI
opengame_ui
ChooseHero
gamescene_ui
gamescene_ui/loading_UI
gamescene_ui/WH_UI
gamescene_ui/WS_UI
gamescene_ui/GameOver_UI
gamescene_ui/CI_UI
gamescene_ui/ST_UI
gamescene_ui/Store_UI
gamescene_ui/EndlessMooe_ui
gamescene_ui/SingleLogin_UI/actions
NPC/n14
```

`EndlessMooe_ui` is recorded exactly as it appears in the original binary; it may be an original typo rather than a transcription error.

## Why this matters

This function proves that the shipped runtime expects a writable/extracted asset tree rather than treating the APK ZIP as the only resource root during normal game startup.

That narrows the resource-reconstruction work to two linked questions:

1. **How is the APK `assets/` tree copied or unpacked into the app files directory?**
2. **At what point are encoded Lua/resource payloads transformed into the representation consumed by the game?**

The native library contains `CCFileUtilsAndroid::uncompressAssets()`, which is an obvious extraction candidate, but its actual startup caller must be confirmed before assigning it that role.

## Next targets

- Trace callers of `CCFileUtilsAndroid::uncompressAssets()`.
- Decompile the Java file-directory helper used by `getFileDirectoryJNI()`.
- Determine whether first-run extraction creates `<files>/assets` directly or another versioned/staged directory that is later renamed.
- Trace `StartLua.lua` after search-path resolution into the file reader and decoder.
- Preserve the search-path ordering in the modern compatibility layer because duplicate resource names may depend on first-match behavior.
