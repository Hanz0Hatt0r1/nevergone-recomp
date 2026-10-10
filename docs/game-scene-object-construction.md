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

The second serialized boolean and later conditional/tail fields remain unresolved here. The recovered common sprite path only passes the byte derived from source `+0x34` to `CCSprite::setFlipX(bool)`; it does not establish a corresponding `setFlipY` use for the second serialized boolean.

The recovered `trailing_i32` is **local child Z**, not a global object-order key. At `GameSceneObject::initWithData()+0xa58`, the receiver is the individual `GameSceneObject`, the child is that object's created sprite, and the recovered integer is passed to the object's `addChild(sprite, z_order)` call. Separate GameSceneObject instances therefore continue to follow the already recovered layer/object traversal order; project-owned direct rendering must not reorder different objects by this local child Z value.

## Proven design resolution, content scale and anchor

The original `AppDelegate::AddAllSearchPath()` obtains the shared `CCEGLView`, loads literal floats `1136.0f` and `640.0f`, sets the policy argument to zero, and dispatches virtual slot `+0x28`. The shipped `CCEGLViewProtocol` vtable resolves that slot to `CCEGLViewProtocol::setDesignResolutionSize(float, float, ResolutionPolicy)` at `0x00533e42`/Thumb `0x00533e43`.

The shipped `setDesignResolutionSize` implementation treats policy values `1..4` as special cases; policy `0` keeps independent X/Y scale factors. This is the Cocos2d-x **ExactFit** path. The GameScene design rectangle is therefore 1136×640 and maps to the entire physical viewport independently on each axis. Aspect-fit/letterboxing is not faithful to this call site.

`CCDirector::init()` writes `1.0f` to the content-scale-factor field at `+0x94`, and no game call site for `CCDirector::setContentScaleFactor(float)` is present. Direct `CCSprite::create(file)` texture dimensions therefore remain 1:1 with design units in the recovered path.

`CCSprite::initWithTexture()` constructs `CCPoint(0.5f, 0.5f)` and dispatches sprite vtable slot `+0x6c`; that slot resolves to `CCSprite::setAnchorPoint(CCPoint const&)`. The ordinary sprite anchor is consequently centered at `(0.5, 0.5)`.

Finally, `CCNode::nodeToParentTransform()` multiplies the stored rotation by the literal `pi/180` through a negative multiply before sine/cosine evaluation. Positive Cocos rotation is therefore clockwise. The direct geometry adapter reproduces this sign rather than using a conventional positive counter-clockwise matrix.

## Direct renderer consistency boundary

The Java stager and native texture cache already publish direct-file pixels transactionally under the deterministic request revision derived from the current render queue. The renderer must preserve that boundary:

- rebuild the direct request snapshot from the current live queue;
- require the GL texture cache `source_revision` to equal that request revision;
- require every direct request to resolve to a valid texture before changing the framebuffer;
- preserve filtered queue order rather than sorting different objects by their local child Z;
- only after complete validation replace the generic entered-game fallback and submit the direct sprite quads.

This prevents a scene transition from mixing a new GameScene queue with stale texture handles from the previous scene.

## `createGSObject()` is a separate boundary

Despite its name, `GameScene::createGSObject()` is not the layer-object renderer loop above. Its recovered call path queries scene action data and creates enemy/NPC objects through battle/NPC managers. The first project-owned visual scene path should therefore be based on `loadingTex()` plus `GameSceneObject::createWithData()/initWithData()`, while action/enemy/NPC creation remains a later boundary.

## Project-owned construction plan

`game_scene_construction_plan` now captures the proven traversal and type-0 construction contract:

- at most the first 11 ordered scene layers are visited, with `z_index` equal to their array index;
- each visited layer keeps its original object order;
- every object exposes the proven `type_code` derived from the parsed leading int32;
- ordinary type-0 visual objects are classified as `type0-sprite-backed` and carry both a resource-selection contract and the proven sprite transform;
- `klhuo-1.png` is classified separately as `type0-scene-action-pair` and does not receive a static sprite transform;
- nonzero object construction remains unresolved;
- the complete evidence-backed `ObjectRecord` remains available without renaming unresolved parser fields;
- source layers beyond index 10 are counted as ignored because the original `loadingTex()` loop never requests them.

The direct-file branch can now be staged and rendered with recovered transform/design semantics. The next visual renderer milestone is resolving the sprite-frame-cache atlas/plist path so default type-0 objects can join those direct sprites without inventing resource layout. Action-pair playback and nonzero object-type construction remain separate milestones.
