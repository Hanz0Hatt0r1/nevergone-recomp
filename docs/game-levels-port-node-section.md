# GameLevels LoadGL_PortNode section evidence

This note records the clean-room stream boundary recovered from the original Never Gone 1.0.9 ARMv7 `GameLevels::LoadGL_PortNode(HPData*, int&)` implementation. It contains no proprietary payload values.

## Function boundary

The shipped ELF symbol is at Thumb address `0x002c2091` (disassembly base `0x002c2090`) with a 720-byte symbol size. The loader receives and advances the same external stream offset used by the preceding GameLevels sections.

## Complete stream order

The function first reads one `uint32 port_node_count`. Each loop iteration then consumes exactly:

1. first string: `uint32 byte_length`, one skipped byte, `byte_length` chars;
2. second string: the same length/gap/payload encoding;
3. two `uint32` values;
4. three one-byte bool values via `HPData::getBytes(bool&, HPRange)`;
5. three more `uint32` values;
6. third string: `uint32 byte_length`, one skipped byte, `byte_length` chars.

After the last record is constructed and retained, the original loop exits directly into `GameLevels::LinkScenePortNode()`. There are no further `HPData::getBytes` calls before the function epilogue, so the final string payload end is the complete serialized-section boundary.

## Reconstructed parser

`game_levels_port_node_section::parse_record_at()` keeps unresolved fields structural and returns the exact next record offset. `parse_section()` validates the count against the 38-byte minimum record size, chains every record transactionally, and returns the exact stream position where linking begins in the original implementation.

Focused host coverage includes a zero-count section, a populated record at a nonzero offset, multiple chained records, truncation with unchanged output, and hostile-count rejection.

## Next step

Chain `LoadGL_Global()` and `LoadGL_PortNode()` into the runtime asset probe. Once all four top-level GameLevels sections are verified end-to-end on the user-owned scene file, move from binary framing into scene-link / first-offline-scene reconstruction without guessing unresolved gameplay semantics.
