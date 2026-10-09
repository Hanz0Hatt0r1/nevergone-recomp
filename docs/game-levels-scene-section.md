# GameLevels LoadGL_Scene section evidence

This note records the clean-room boundary for the complete scene loop in `GameLevels::LoadGL_Scene()`. It contains no proprietary scene payload values.

## Scene-loop control flow

The top-level prefix contains the recovered signed format gate followed by `scene_count`. When `scene_count` is nonzero, each scene begins with the already proven string-length/string, two-float point and `layer_count` header, followed by exactly `layer_count` complete layer records.

After the final layer of a scene, original ARMv7 reaches `GameSceneData::AddLayer()` and then the scene-add call around normalized Ghidra `0x002d2ef0`. The scene-loop index is incremented and compared directly with the top-level `scene_count`. When more scenes remain, control branches directly back to the next scene header around `0x002d282a`. No HPData bytes are consumed between the previous scene's final layer join and the next scene header.

When the last scene is added, `LoadGL_Scene()` exits the scene loop. A zero `scene_count` takes the same section-exit path without consuming any scene-local bytes.

## Generic scene parser

`game_levels_layer_tail::parse_scene_record_at()` accepts an explicit byte offset plus the recovered top-level format gate. It parses:

1. uint32 string byte length;
2. the one-byte skipped gap and exact string payload;
3. two floats forming the recovered scene point;
4. uint32 layer count;
5. exactly that many complete generic layer records.

`end_offset` is the exact next scene start / scene-loop join. `parse_scene_section()` first parses the 8-byte top-level prefix, then chains scene-record end offsets for exactly `scene_count` records. A zero count succeeds at byte 8. Conservative 17-byte minimum scene and 16-byte minimum layer sizes are used only to reject impossible counts before vector reservation; variable records are always advanced by their actual parsed end offsets.

## Runtime boundary

The imported pvp-scene probe exposes `scene_section_readable` and only records its verified byte count. `cpp_OnEnterGame` reports the evidence state as `scene-section-verified`. Parsed proprietary scene strings, points, object values and borders are not emitted in diagnostics.

## Next step

`LoadGL_Scene()` is now structurally bounded through its complete scene loop. The next format section in the recovered `GameLevels::LoadGameLevels()` order is `LoadGL_Actions()`. Reconstruction should begin at that function's top-level count/control-flow boundary and continue with the same explicit-offset, transactional approach before attempting `LoadGL_Global()` or `LoadGL_PortNode()`.
