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

When the copied type code is zero, `GameSceneObject::initWithData()` enters a sprite-backed path. It reads the object string copied from `GameSceneLayerObjectData + 0x18`, compares several special names, and reaches `cocos2d::CCSprite::create(char const*)`, `createWithSpriteFrame(...)`, or related sprite handling. This is sufficient to classify parsed type `0` records as following the original sprite-backed construction path. The string is therefore retained structurally, but the project does not yet assume that every special-case type-0 object uses that exact string as its final texture lookup.

No equivalent project-owned semantic classification is assigned yet to nonzero object types. In particular, `loadingTex()` has special pre-construction handling for type `6`, but that behavior depends on additional game-mode/player state and is not reconstructed here.

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

The vtable targets are not inferred from slot order alone: the shipped `CCSprite` table resolves the relevant indirect targets to `setPosition`, `setRotation`, `setScaleX`, and `setScaleY`; `setFlipX` is called directly. The `GameSceneObject` inherited `CCNode` vtable target used after sprite setup resolves to `CCNode::addChild(CCNode*, int)`, proving that the copied integer is the child Z-order.

The parser keeps its structural historical member names so binary parsing remains independent of gameplay/rendering interpretation. `game_scene_construction_plan::Type0SpriteTransform` is the first renderer-facing structure that exposes these proven semantics as `position`, `rotation`, `scale`, `flip_x`, and `child_z_order`.

The second serialized boolean and later conditional/tail fields remain unresolved here. They must not be assigned renderer semantics until another original use proves them.

## `createGSObject()` is a separate boundary

Despite its name, `GameScene::createGSObject()` is not the layer-object renderer loop above. Its recovered call path queries scene action data and creates enemy/NPC objects through battle/NPC managers. The first project-owned visual scene path should therefore be based on `loadingTex()` plus `GameSceneObject::createWithData()/initWithData()`, while action/enemy/NPC creation remains a later boundary.

## Project-owned construction plan

`game_scene_construction_plan` captures only the proven traversal and construction contract:

- at most the first 11 ordered scene layers are visited, with `z_index` equal to their array index;
- each visited layer keeps its original object order;
- every object exposes the proven `type_code` derived from the parsed leading int32;
- only type `0` is classified as `type0-sprite-backed`;
- type-0 objects expose an evidence-backed `Type0SpriteTransform` containing position, rotation, X/Y scale, horizontal flip, and child Z-order;
- the complete evidence-backed `ObjectRecord` is retained so later adapters can use additional proven fields without renaming unresolved parser values;
- source layers beyond index 10 are counted as ignored because the original `loadingTex()` loop never requests them.

The next renderer step can consume this transform contract while separately recovering the special-case type-0 resource-selection rules. Anchor-point semantics, the second boolean, and nonzero object-type construction remain intentionally unresolved.
