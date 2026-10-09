# GameLevels LoadGL_Actions section evidence

This note records the clean-room binary boundary recovered for `GameLevels::LoadGL_Actions()`. It contains no proprietary action payload values.

## Entry and loop boundary

The original 1.0.9 ARMv7 function is located at `0x002c198c` in the shipped library (`0x002d198c` in the normalized checked-in Ghidra address space). The function receives the same external stream offset by reference that `LoadGL_Scene()` advances.

At entry it reads one `uint32` from the current offset, advances the offset by four bytes, then compares a zero-based loop index directly with that value before constructing any action-layout object. The value is therefore the action-record count. A zero count consumes only those four bytes and exits the section.

## Complete action record

Each loop iteration consumes the following stream sequence in order:

1. `uint32` first string byte length;
2. advance five bytes from the length-field start, proving the existing four-byte length plus one skipped byte;
3. copy exactly that many chars and advance by the payload length;
4. repeat the same `uint32 + one-byte gap + payload` sequence for a second string;
5. one `uint32`;
6. one `uint32`;
7. one `int32`;
8. two `float` values, assigned together as a Cocos point;
9. when the recovered top-level GameLevels format gate is `> 1`, read two additional `int32` values;
10. when that same gate is `> 3`, read two further `int32` values.

After those reads, the original only creates/retains the two strings, assigns the recovered scalar/point fields, applies an in-memory default when one gated value is zero, adds the action object to the owning array, increments the loop index and either begins the next record immediately or exits. There are no further `HPData::getBytes` calls in the record tail.

The clean-room parser intentionally keeps structural field names where gameplay semantics are not yet proven. It also preserves the raw zero value instead of reproducing the original in-memory default substitution, because that substitution does not consume stream bytes.

## Reconstructed parser

`game_levels_actions_section::parse_action_record_at()` accepts an explicit start offset and the already recovered signed format gate. It is transactional and returns the exact `end_offset` after the complete record.

`parse_section()` begins at the action-count field, rejects impossible counts before reserving memory, chains exact record end offsets and returns the stream position handed to the next `LoadGameLevels()` section. Synthetic host coverage exercises zero records, multiple records, nonzero section offsets, both format gates, truncation and hostile counts.

## Next step

With `LoadGL_Scene()` and `LoadGL_Actions()` structurally bounded, the next section in the recovered `GameLevels::LoadGameLevels()` order is `LoadGL_Global()` at normalized address `0x002d1c5c`. Its count/control-flow boundary should be reconstructed next before attempting `LoadGL_PortNode()`.
