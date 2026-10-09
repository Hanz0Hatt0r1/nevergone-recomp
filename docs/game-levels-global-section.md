# GameLevels LoadGL_Global section evidence

This note records the clean-room stream boundary recovered from the original Never Gone 1.0.9 ARMv7 `GameLevels::LoadGL_Global(HPData*, int&)` implementation. It contains no proprietary payload values.

## Function boundary

The shipped ELF symbol is at Thumb address `0x002c1c5d` (disassembly base `0x002c1c5c`) with a 968-byte symbol size. The project address convention maps this to normalized `0x002d1c5c`.

The loader receives the same external stream offset by reference as the preceding scene/action loaders and advances it after every `HPData::getBytes` call.

## Complete stream order

For a non-null `HPData*`, the function consumes:

1. `uint32 string_count`;
2. `string_count` records, each encoded as `uint32 byte_length`, one skipped byte, then exactly `byte_length` chars;
3. two standalone `uint32` values;
4. `uint32 int_int_float_count` followed by that many records of `int32`, `int32`, `float`;
5. `uint32 uint_pair_count` followed by that many records of `uint32`, `uint32`;
6. `uint32 enemy_count` followed by that many records of `int32`, `int32`, `float`, then five more `int32` values.

After the last enemy record, control goes directly to the common function epilogue. There are no further stream reads in `LoadGL_Global()`.

The original performs gameplay-side assignments while consuming these values (including accumulated float state and object creation). The clean-room parser deliberately records only the proven serialized structure and does not reproduce those semantics yet.

## Reconstructed parser

`game_levels_global_section::parse_section()` accepts an explicit absolute start offset, validates every count against remaining bytes before allocation, parses all variable-width strings transactionally, and returns the exact `end_offset` for the following `LoadGL_PortNode()` call.

Focused host coverage includes the all-empty-loop minimum, a nonzero-offset section with every loop populated, truncation with transactional output, and a hostile count.

## Next step

Chain this section after the verified `LoadGL_Actions()` end offset in the runtime asset probe. Then reconstruct the final `LoadGL_PortNode()` stream layout from its ELF symbol at `0x002c2091` before attempting scene-link semantics.
