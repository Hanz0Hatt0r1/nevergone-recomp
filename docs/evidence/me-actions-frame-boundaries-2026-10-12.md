# MeActions frame-boundary advancement — 2026-10-12

## Clean-room boundary

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records bounded behavior from `MeActionsData::loadWBGFileWithHPData()` and `MeActionsSystem::waUpdate(float)`. No original binary, action payload, image, screenshot or disassembly listing is committed.

## Section-F projection

`MeActionsData::init()` allocates retained `CCArray` objects at data offsets `+0xb0` and `+0xb4`.

Near the end of `loadWBGFileWithHPData()` the native loader iterates exactly the primary-frame count. Each iteration consumes the same 0x18-byte Section-F tuple already parsed by `enemy_actions_wbg_combo_section`:

- four signed int32 values;
- two float32 values.

For the player loader:

- tuple int32[3] is incremented by one and drives run boundaries appended to `MeActionsData +0xb0`;
- tuple int32[1] is incremented by one and drives run boundaries appended to `MeActionsData +0xb4`;
- each emitted `ActionComboValue` stores its source tuple index at object `+0x18`;
- the final tuple always closes the current run.

This is the same run-boundary construction already recovered for the shared serialized Section F as enemy arrays `+0x94` and `+0x98`. The project therefore projects the existing parsed `array_94_values` into player `+0xb0`, and `array_98_values` into player `+0xb4`, changing only the destination identity.

## `waUpdate()` boundary use

After the timing threshold is consumed, `MeActionsSystem::waUpdate()` reads the current `+0xb0/+0xb4` entries and uses only their `ActionComboValue +0x18` frame boundary in this branch.

For a non-empty `+0xb0` array:

1. increment current player frame `+0x1a4`;
2. when `+0x201` is enabled and current frame is greater than or equal to the current `+0xb0` boundary, increment `+0x1f8`;
3. clamp `+0x1f8` to the final boundary entry instead of moving one past the array;
4. if another primary boundary entry exists, continue playback;
5. otherwise continue while current frame is less than or equal to the final boundary;
6. once current frame is greater than the final boundary, set `+0x200 = 1`, set `+0x220 = 1`, retain `boundary - 1` in `+0x1f4`, and reset current frame to zero.

The secondary `+0xb4` cursor is `+0x1fc`. In the primary-boundary branch it advances when current frame is greater than or equal to its boundary and is likewise clamped to the final entry.

If `+0xb0` is empty, the fallback path uses the primary frame-array count as its completion boundary. In that branch the `+0xb4` cursor advances only when current frame is strictly greater than its boundary. The project preserves this native `>=` versus `>` difference.

## Same-call iteration

After frame processing the native function compares previous frame `+0x1a8` with current frame `+0x1a4`. When they differ it optionally calls `updateData()` through the recovered `+0x1bf` gate, copies current to previous, and loops back to the timing comparison without adding the caller's delta again.

Therefore one `waUpdate(delta)` may advance several frames when the retained millisecond accumulator exceeds several consecutive frame thresholds. `apply_wa_update_playback()` mirrors that behavior by applying the supplied delta only on the first timing pass and consuming the remaining accumulator with zero additional delta on subsequent iterations.

## Project boundary

`me_actions_combo_boundaries` now provides the player `+0xb0/+0xb4` projection from the already parsed Section-F block. `me_actions_playback_timing` now models the recovered frame cursor, boundary cursors, same-call iteration and `done_220` transition.

The next integration boundary is to retain this cursor per loaded live player action and feed its changing `current_frame_1a4` into the existing `game_scene_player_frame_request`/texture staging path. That will turn action33/A/B from first-frame swaps into live multi-frame playback and will provide the first evidence-backed action-completion signal for return-to-idle logic.
