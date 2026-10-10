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

When the copied type code is zero, `GameSceneObject::initWithData()` enters a sprite-backed path. It reads the object string copied from `GameSceneLayerObjectData + 0x18`, compares a few special names, and reaches `cocos2d::CCSprite::create(char const*)` or related sprite handling. This is sufficient to classify parsed type `0` records as following the original sprite-backed construction path.

No equivalent project-owned semantic classification is assigned yet to nonzero object types. In particular, `loadingTex()` has special pre-construction handling for type `6`, but that behavior depends on additional game-mode/player state and is not reconstructed here.

## `createGSObject()` is a separate boundary

Despite its name, `GameScene::createGSObject()` is not the layer-object renderer loop above. Its recovered call path queries scene action data and creates enemy/NPC objects through battle/NPC managers. The first project-owned visual scene path should therefore be based on `loadingTex()` plus `GameSceneObject::createWithData()/initWithData()`, while action/enemy/NPC creation remains a later boundary.

## Project-owned construction plan

`game_scene_construction_plan` captures only the proven traversal contract:

- at most the first 11 ordered scene layers are visited, with `z_index` equal to their array index;
- each visited layer keeps its original object order;
- every object exposes the proven `type_code` derived from the parsed leading int32;
- only type `0` is classified as `type0-sprite-backed`;
- the complete evidence-backed `ObjectRecord` is retained so later adapters can use proven fields without renaming unresolved values;
- source layers beyond index 10 are counted as ignored because the original `loadingTex()` loop never requests them.

The next renderer step should determine how the already parsed point/scalar fields are applied to the resulting Cocos node before assigning names such as position, anchor, scale, or rotation. Until that downstream use is proven, those fields remain structural.
