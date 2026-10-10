# GameScene object-construction evidence

This note records clean-room control-flow and field-use evidence from the user's original Never Gone 1.0.9 ARMv7 library. It intentionally records only behavior needed by the project-owned reconstruction; it does not contain original game assets or decompiler output.

## Relevant original functions

The shipped Thumb symbols resolve to these code starts:

- `GameScene::loadingTex(cocos2d::CCObject*)` — `0x00346678`
- `GameSceneObject::createWithData(GameSceneLayerObjectData*)` — `0x0034d284`
- `GameSceneObject::initWithData(GameSceneLayerObjectData*)` — `0x003500e0`
- `GameScene::createGSObject()` — `0x00343e5c`
- `GameSceneData::GetSceneLayerDataWithZ(int)` — `0x0035396e`

The exported symbol values are odd because these functions are Thumb code; the addresses above are the aligned instruction starts.

## Layer traversal semantics

`GameScene::loadingTex()` initializes a layer selector to zero, calls `GameSceneData::GetSceneLayerDataWithZ(selector)`, increments it after each layer pass, and continues while the selector is less than 11. The original therefore attempts exactly the slots `0..10`.

`GameSceneData::GetSceneLayerDataWithZ(int)` does not search for a serialized Z field. It obtains the scene layer `CCArray`, checks `count() > index`, and returns `objectAtIndex(index)`. Consequently the Z selector used by this runtime path is the **ordered layer-array index**. The serialized layer `first_float` remains semantically unresolved and must not be relabeled as Z.

When a layer exists, `loadingTex()` iterates its object array by index via `GameSceneLayerData::GetLayerObjectWithIdx(unsigned int)`. Project-owned construction can therefore preserve the parsed layer/object vector order directly.

## Proven object type field

For every layer object, `loadingTex()` reads the 32-bit value at `GameSceneLayerObjectData + 0x14`, writes the current layer index to another object field, and branches on the `+0x14` value before calling `GameSceneObject::createWithData()`.

`GameSceneObject::initWithData()` copies that same `+0x14` value into its owned object-data structure and immediately branches on the copied value. The serialized leading `ObjectRecord::first_i32` feeds this field, so it is now evidence-backed to call this value the **object type code** even though the parser keeps the historical member name for source compatibility.

## Type 0 construction boundary

When the copied type code is zero, `GameSceneObject::initWithData()` enters the recovered type-0 construction family. Most names are resolved through `CCSpriteFrameCache::spriteFrameByName()` and `CCSprite::createWithSpriteFrame()`, while several exact-name branches bypass the cache or perform action-system construction instead.

No equivalent project-owned semantic classification is assigned yet to nonzero object types. In particular, `loadingTex()` has special pre-construction handling for type `6`, but that behavior depends on additional game-mode/player state and is not reconstructed here.

## Proven type-0 resource selection

The following exact control-flow is recovered from `GameSceneObject::initWithData()`:

- the default type-0 visual path calls `CCSpriteFrameCache::sharedSpriteFrameCache()`, then `spriteFrameByName(serialized_name)`, then `CCSprite::createWithSpriteFrame(...)`;
- `czyanwu1.png` and `czyanwu3.png` branch before that default and call `CCSprite::create(serialized_name)` directly;
- `gktianchong.png` and `gkyuanjing.png` also branch to a direct `CCSprite::create(serialized_name)` path;
- `klhuo-1.png` does not construct the ordinary static sprite at this point. It builds two `SceneActionsSystem` instances using the formatted names `se01_born%02d.actData` and `se01_i%02d.actData`.

`game_scene_type0_resource::select()` encodes only these proven decisions. Names whose later special behavior is known but whose resource choice is still the default frame-cache path remain `sprite-frame-by-name`; this keeps animation/visibility behavior separate from texture lookup.

## Proven type-0 sprite transform mapping

The common sprite-finalization block in `GameSceneObject::initWithData()` resolves the Cocos virtual calls through the shipped `CCSprite` vtable. This establishes the following field uses for a created sprite:

| Parsed `ObjectRecord` field | Original object-data offset | Proven downstream call |
| --- | ---: | --- |
| `first_point_x`, `first_point_y` | `+0x1c` | `CCSprite::setPosition(CCPoint const&)` |
| `middle_float` | `+0x24` | `CCSprite::setRotation(float)` |
| `second_point_x` | `+0x28` | `CCSprite::setScaleX(float)` |
| `second_point_y` | `+0x2c` | `CCSprite::setScaleY(float)` |
| `first_bool` | `+0x34` | `CCSprite::setFlipX(bool)` |
| `trailing_i32` | `+0x30` | second argument of `CCNode::addChild(sprite, z_order)` |

The parser keeps its structural historical member names so binary parsing remains independent of gameplay/rendering interpretation. `game_scene_construction_plan::Type0SpriteTransform` is the renderer-facing structure that exposes these proven semantics. It is emitted only for type-0 branches that actually construct a sprite; the `klhuo-1.png` action-pair branch intentionally has no sprite transform.

The second serialized boolean and later conditional/tail fields remain unresolved here. The second boolean is not used as `flipY` on the recovered type-0 sprite-finalization path; only the `+0x34` byte is copied into the field later passed to `CCSprite::setFlipX(bool)`.

## Proven GameScene design-space geometry

The original `AppDelegate::AddAllSearchPath()` obtains the shared `CCEGLView`, loads the literal floats `1136.0f` and `640.0f`, sets policy register `r3` to zero, and dispatches vtable slot `+0x28`. The shipped `CCEGLViewProtocol` vtable resolves that exact slot to `setDesignResolutionSize(float, float, ResolutionPolicy)`. In the implementation, policy values `1..4` take special scaling branches while policy `0` keeps independent X/Y scales, matching Cocos2d-x `ExactFit`. The project can therefore use a **1136×640 ExactFit design space** for this scene path without inferring it from asset dimensions.

`CCDirector::init()` writes `1.0f` to its content-scale-factor field at `+0x94`, and no game call to `CCDirector::setContentScaleFactor(float)` is present. Direct-file texture pixel dimensions therefore remain 1:1 with design units on this path.

`CCSprite::initWithTexture()` constructs `CCPoint(0.5f, 0.5f)` and dispatches sprite vtable slot `+0x6c`; that slot resolves to `CCSprite::setAnchorPoint(CCPoint const&)`. The default direct sprite anchor is consequently the center `(0.5, 0.5)`.

`CCNode::nodeToParentTransform()` multiplies the stored rotation by `-pi/180` before its sine/cosine transform. Positive serialized/Cocos rotation is therefore clockwise. `game_scene_direct_sprite_geometry` reproduces that sign, applies scale in local sprite space, translates by the proven position, then maps 1136×640 ExactFit coordinates directly to clip space.

Android `Bitmap.getPixels()` exposes the top source row first, while the project's GLES upload retains that row at texture coordinate `v=0`; direct sprite quads therefore use `v=0` for top vertices and `v=1` for bottom vertices. Proven `flipX` swaps only U coordinates, matching Cocos sprite behavior rather than negating geometry.

## `createGSObject()` is a separate boundary

Despite its name, `GameScene::createGSObject()` is not the layer-object renderer loop above. Its recovered call path queries scene action data and creates enemy/NPC objects through battle/NPC managers. The first project-owned visual scene path should therefore be based on `loadingTex()` plus `GameSceneObject::createWithData()/initWithData()`, while action/enemy/NPC creation remains a later boundary.

## Project-owned construction and direct render path

The project-owned GameScene path now captures the proven traversal, resource, transform, staging and direct rendering contract:

- at most the first 11 ordered scene layers are visited, with `z_index` equal to their array index;
- each visited layer keeps its original object order;
- every object exposes the proven `type_code` derived from the parsed leading int32;
- ordinary type-0 visual objects are classified as `type0-sprite-backed` and carry both a resource-selection contract and the proven sprite transform;
- `klhuo-1.png` is classified separately as `type0-scene-action-pair` and does not receive a static sprite transform;
- direct-file names resolve only beneath the imported `assets/gamescene/gs_res_image_file/` root;
- Java stages the live direct-file request revision into an atomic native ARGB pixel store;
- the GL texture cache transactionally replaces a texture revision only after every requested texture succeeds;
- `game_scene_direct_sprite_geometry` builds centered, rotated, scaled and horizontally flipped quads in the recovered 1136×640 design space;
- the direct renderer draws only when the current render-queue revision and active GL texture revision match completely, so a stale scene cannot be mixed with current textures;
- frame-cache atlas sprites, action-pair playback and nonzero object construction remain unresolved and are not synthesized.

The next visual milestone is recovering and staging the sprite-frame-cache atlas/plist resources so default type-0 objects can interleave with the already rendered direct-file sprites. Enemy/NPC/action construction remains the separate `createGSObject()` boundary.
