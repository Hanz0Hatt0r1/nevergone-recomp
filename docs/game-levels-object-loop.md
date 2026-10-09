# GameLevels first-layer object loop evidence

This note records the clean-room evidence boundary reached after reconstructing one complete `GameSceneLayerObjectData` record. It contains no proprietary scene data.

## Sequential object records

Original ARMv7 `GameLevels::LoadGL_Scene()` maintains one byte offset through the current layer's object loop. The recovered `object_count` controls the loop. After a complete object reaches the common `GameSceneLayerData::AddObject()` join, the loop index increments and the next iteration begins parsing another object from the current stream offset.

Because the complete record tail is now recovered for the observed type branches, the end offset of one object is an evidence-backed start offset for the next object. Record width is still variable; it is never treated as a fixed stride.

## Generic parser boundary

`parse_object_record_at()` accepts:

- the bounded `hp_data::Reader`;
- an explicit absolute start offset;
- the already recovered top-level signed format gate;
- an output `ObjectRecord`.

It reproduces the verified object stream shape: core string/float/int/bool fields, the top-level `> 2` counted uint32 vector, the nonzero-object conditional header, and the type-specific tail for `4/6/9/10`. On success, `end_offset` is the exact byte position immediately after that record's final stream read. The output is transactional on failure.

`parse_first_layer_objects()` chains those end offsets for exactly the recovered first-layer `object_count`. A 35-byte minimum core size is used only as a conservative pre-reserve rejection for impossible/hostile counts; it is not used as the actual record stride.

Host regressions cover two heterogeneous sequential records, an empty object loop, a declared third record that is not present, a hostile object count, invalid start offsets, and the top-level `> 2` version-vector path.

## Runtime boundary

The imported `pvp_scene.glData` probe now reports success only when the complete first-layer object loop is readable. `cpp_OnEnterGame` exposes that evidence state as `first-layer-objects-verified` and records the absolute verified byte count without logging proprietary parsed values.

## Next boundary

The parser intentionally stops immediately after the final object in the first layer. The next `LoadGL_Scene()` bytes belong to post-object layer data. Focused ARMv7 analysis should recover the counted point-vector blocks and their exact joins before parsing a second layer or advancing to a second scene.
