# GameLevels version-gated object list

ARMv7 evidence identifies the first top-level signed `int32` read by `GameLevels::LoadGL_Scene()` as a serialized format/version gate: it is stored at `GameLevels+0x3c` and later compared against `1` and `2` to select additional fields.

Immediately after the verified first-object core, the original checks this value against `2`. For versions `>2` it reads one `uint32` count, then loops exactly that many times, reading one `uint32` per iteration and appending it to the object's `std::vector<uint32_t>` storage. Versions `<=2` skip the entire block and consume no bytes.

`parse_first_object_versioned_list()` reproduces only this proven structure. The list's gameplay meaning remains unnamed. The parser validates `count <= remaining_bytes / 4` before reserving or reading entries and updates caller output only after the complete block is available.

The next serialized block is gated by the first object's initial `int32`; it is deliberately outside this increment.
