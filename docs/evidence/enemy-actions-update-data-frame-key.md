# EnemyActions `updateData()` frame-key evidence

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Section-A string-to-AFD mapping

A tighter disassembly of `EnemyActionsData::loadWBGFile` resolves the remaining primary-record string mapping.

After the fixed Section-A fields, native parses three framed strings. During `ActionFrameData` construction around `0x28f7d8..0x28f818`:

- the **first** parsed string is converted to `CCString`, retained, and stored at `ActionFrameData+0x60`;
- its signed serialized length is stored at `ActionFrameData+0x64`;
- the **third** parsed string is converted to `CCString`, retained, and stored at `ActionFrameData+0x68`;
- its signed serialized length is stored at `ActionFrameData+0x6c`.

The second parsed Section-A string is not the value placed in `+0x68`; no claim about its broader meaning is made here.

Therefore the project-owned mapping is:

- `ActionFrameRecord::first_string` -> native `AFD+0x60`;
- `ActionFrameRecord::first_string_length_i32` -> native `AFD+0x64`;
- `ActionFrameRecord::third_string` -> native `AFD+0x68`;
- `ActionFrameRecord::third_string_length_i32` -> native `AFD+0x6c`.

This also matches the genuine APK observations where the first string is a plist-like resource name while the third string is a frame-like PNG key such as `empty_a01_001.png`.

## `spriteFrameByName()` request

In the `updateData()` tail, after the current primary frame is reached, native executes the bounded sequence at `0x2ac476..0x2ac48a`:

1. `CCSpriteFrameCache::sharedSpriteFrameCache()`;
2. load selected `ActionFrameData+0x68`;
3. `CCString::getCString()`;
4. `CCSpriteFrameCache::spriteFrameByName(...)`;
5. retain the returned pointer in a register for later rendering/object logic.

The request itself is unconditional once the recovered updateData entry preconditions and valid current-frame selection are satisfied. This slice does not establish what later code does with a null or non-null returned sprite-frame pointer.

## Reconstruction

`apply_update_data_frame_lookup(document, state)` reuses the recovered updateData readiness/current-frame entry contract and reports:

- the selected native `+0x68` key from `ActionFrameRecord::third_string`;
- the native `+0x6c` serialized length from `third_string_length_i32`;
- whether the original reaches the `spriteFrameByName()` request.

No cocos2d cache lookup is performed by the helper. Empty frame names are preserved and still reported as a lookup request because the native sequence calls `getCString()` and `spriteFrameByName()` without an observed empty-string guard.

## Outside scope

The later branches involving the returned sprite frame, the `+0x250 == 0` virtual object path, enemy cut-object arrays, system `+0x27c`, movement, position, visibility and rendering writes are not reconstructed by this helper.

No proprietary payload or decompiler-derived source is included.
