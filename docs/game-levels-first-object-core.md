# GameLevels first-object core evidence

This note advances the bounded `GameLevels::LoadGL_Scene()` reconstruction beyond the historical first-object 8-byte prefix. It records structural evidence only; no gameplay meaning is assigned where the original use does not prove it.

## ARMv7 range construction

The original Android 1.0.9 `libcocos2dcpp.so` places `GameLevels::LoadGL_Scene()` at `0x002c27b8` (the checked-in Ghidra addresses are `+0x10000`). Around the first object body, the verified sequence is:

1. `getBytes(int*, HPRange)` at `0x002c29a4` with a 4-byte range;
2. advance the external stream offset by 4;
3. `getBytes(unsigned int*, HPRange)` at `0x002c29bc` with a 4-byte range;
4. load that uint32 as the next range length;
5. advance the external stream offset by **5** from the uint32 field start;
6. call `getBytes(char*, HPRange)` at `0x002c29e6` with `{offset_after_uint32 + 1, uint32_value}`;
7. advance by exactly that payload length and append a NUL at `buffer[length]`.

Therefore the historical `FirstObjectPrefix::second_u32` is conclusively the byte length of the immediately following string payload. As in the first-scene header, one additional byte between the length and payload is skipped but remains semantically unnamed.

## Following scalar sequence

Immediately after the string, the original performs five consecutive 4-byte float reads, followed by one 4-byte signed integer and two one-byte boolean reads. Assignment shape proves:

- float 1 + float 2 are copied as one `CCPoint`;
- float 3 is stored independently;
- float 4 + float 5 are copied as a second `CCPoint`;
- the following int32 is stored independently;
- the two booleans are stored as adjacent one-byte fields.

This proves structure, not gameplay names such as position, scale, anchor, rotation, or flags. The clean-room API therefore exposes neutral names: `first_point`, `middle_float`, `second_point`, `trailing_i32`, `first_bool`, `second_bool`.

## Clean-room boundary

`game_levels_scene_prefix::parse_first_object_core()` now parses transactionally through:

`int32 + uint32(length) + skipped byte + string[length] + 5*float + int32 + bool + bool`

For an object string of `N` bytes, this object core occupies `35 + N` bytes beginning at the first object record. Parsing stops immediately after the second bool, before the subsequent conditional block seen in `LoadGL_Scene()`.

The prior `parse_first_object_prefix()` remains available as the weaker 8-byte milestone. Runtime readiness now requires the stronger core boundary.

## Next boundary

The next code after the second bool conditionally reads additional uint32 values depending on scene/object state. That block is not reconstructed here. Its loop/count semantics and ownership must be proven before the parser advances again.
