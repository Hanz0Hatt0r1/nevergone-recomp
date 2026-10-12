# GameScene `gkanniu2.png` empty-resource substitution (2026-10-12)

Target: clean-room reconstruction of the user-owned Never Gone 1.0.9 ARMv7 runtime. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

No original binary or proprietary image is stored in the repository. This note records only bounded control-flow/resource-selection metadata needed by the project-owned renderer.

## Recovered branch

`GameSceneObject::initWithData(GameSceneLayerObjectData*)` starts at aligned Thumb address `0x003500e0`.

Inside the type-0 path, the ordinary resource is first constructed through `CCSpriteFrameCache::spriteFrameByName(serialized_name)` and `CCSprite::createWithSpriteFrame(...)`. The subsequent exact-name chain compares the same serialized name against several special values.

For `gkanniu2.png`:

- the comparison occurs around `0x00350580..0x0035058e`;
- an equal comparison falls through to `0x00350590`;
- the literal loaded there resolves to `empty.png`;
- `0x00350596` calls `cocos2d::CCSprite::create(char const*)` with that literal;
- the returned sprite replaces the previously stored sprite pointer at `GameSceneObject + 0x10c`;
- the next virtual dispatch uses object-vtable slot `+0x80` with boolean argument `false`;
- that slot resolves through the shipped `cocos2d::CCSprite` vtable to `cocos2d::CCSprite::setVisible(bool)`.

Therefore the live object for this exact serialized name is an `empty.png` sprite that is immediately hidden. It is neither the serialized `gkanniu2.png` atlas frame nor a visible fallback quad.

The same block also shows a generic null-sprite fallback to `CCSprite::create("empty.png")` followed by `setVisible(false)` after the special-name chain. That broader failure fallback is recorded here but is not yet used to weaken the clean-room atlas-staging acceptance boundary.

## Imported location

The user-owned original APK contains the fallback as `assets/empty.png`. `OriginalApkImporter` strips the APK `assets/` prefix, so the decoded project-owned import appears as `filesDir/assets/empty.png`.

This differs from the other recovered direct type-0 files (`czyanwu1.png`, `czyanwu3.png`, `gktianchong.png`, `gkyuanjing.png`), which resolve through the recovered `gamescene/gs_res_image_file` search directory.

## Clean-room consequence

`game_scene_type0_resource::select("gkanniu2.png")` records `empty.png` together with `initially_visible=false`. The construction plan retains that state, while the direct render queue omits initially hidden sprites before texture-request accounting.

This reproduces the observable original consequence: no visible quad is submitted and no `gkanniu2.png` atlas-frame dependency remains. The generic original null-frame fallback remains a separate reconstruction step.
