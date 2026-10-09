# GameLevels first-layer tail evidence

This note records the clean-room boundary immediately after a layer's object loop in `GameLevels::LoadGL_Scene()`. It contains no proprietary scene payload values.

## Recovered stream shape

After the object-loop `AddObject()` join at normalized Ghidra `0x002d2dbe`, the original exits the loop and performs two structurally identical counted point-list parses.

First list:

1. read one `uint32` count at Ghidra `0x002d2dea`;
2. for each entry, read two floats at `0x002d2e20` and `0x002d2e36`;
3. construct a `CCPoint` and pass it through `LayerBorderPoint::createWithPoint()`;
4. add it with `LayerBorderPointClass::addTopBorderPoint()` at `0x002d2e54`.

Second list:

1. read one `uint32` count at `0x002d2e70`;
2. for each entry, read two floats at `0x002d2ea2` and `0x002d2eb8`;
3. construct a `CCPoint` and pass it through the same `LayerBorderPoint` factory;
4. add it with `LayerBorderPointClass::addBottomBorderPoint()` at `0x002d2ed6`.

When the second loop finishes, the next call is `GameSceneData::AddLayer()` at `0x002d2ee0`. The layer-loop index is incremented immediately afterward and control returns to the recovered layer-count comparison. No additional HPData bytes are consumed between the second point list and `AddLayer()`.

## Reconstructed boundary

`game_levels_layer_tail::parse_first_layer_record()` starts at the completed first-layer object-loop byte count, reads both counted lists transactionally, and returns the exact byte offset corresponding to the original `AddLayer()` join. Counts are checked against remaining bytes before vector reservation. The first list reserves room for the mandatory second count field as part of its bounds check.

The imported pvp-scene probe and `cpp_OnEnterGame` transition expose this as `first-layer-record-verified` without logging proprietary point values.

## Next step

A complete layer record now has an evidence-backed variable byte width. The next step is to generalize this layer parser to an explicit start offset plus top-level format gate, then chain those record ends across the recovered scene `layer_count`. Once the full first scene's layer loop is proven, the parser can advance to the next scene using the same sequential stream-offset discipline.
