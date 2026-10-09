# GameLevels first-scene layer-loop evidence

This note records the clean-room boundary reached after completing one layer record in `GameLevels::LoadGL_Scene()`. It contains no proprietary scene payload values.

## Layer loop control flow

After the second border-point list, original ARMv7 calls `GameSceneData::AddLayer()` at normalized Ghidra `0x002d2ee0`. It immediately increments the layer-loop index, compares it with the previously recovered scene `layer_count`, and branches back to the same layer header at `0x002d2906` while more layers remain.

No HPData bytes are consumed between `AddLayer()` and the next layer header. After the final layer, control falls through directly to the scene-add call and scene-loop increment.

## Generic layer record

`game_levels_layer_tail::parse_layer_record_at()` reconstructs the complete variable-width record from an explicit byte offset:

1. one structural float;
2. one `uint32` object count;
3. exactly that many complete generic object records;
4. the counted top-border CCPoint list;
5. the counted bottom-border CCPoint list.

Its `end_offset` corresponds to the original `AddLayer()` join. A conservative 35-byte minimum object core is used only to reject impossible object counts before allocation; no fixed object stride is assumed.

`parse_first_scene_layers()` starts at the verified first-scene header boundary and chains complete layer end offsets for exactly the recovered `layer_count`. A zero layer count is valid and consumes no layer-local bytes.

## Runtime boundary

The imported scene probe and enter-game transition expose a successful first-scene layer loop as `first-scene-layers-verified`. Diagnostics retain only verified byte counts and do not print parsed proprietary values.

## Next step

The first scene now has a complete evidence-backed byte extent. The next step is to generalize the scene parser itself to an explicit start offset, then chain scene record ends across the top-level `scene_count`. The original control flow shows no extra HPData reads between the final `AddLayer()` and the next scene header, but the generic scene wrapper should preserve the same bounded, transactional discipline before the parser advances into `LoadGL_Actions()`.
