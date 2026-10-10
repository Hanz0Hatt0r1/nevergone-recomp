# EnemyActions `updateData()` resource-selection evidence

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note continues the bounded `updateData()` entry contract after a valid primary `ActionFrameData` has been selected.

## `ActionFrameData+0x60`

`EnemyActionsData::loadWBGFile` creates the Section-A `ActionFrameData` objects. Around `0x28f77c..0x28f7e4`, the first parsed Section-A string is converted to a `CCString`, retained, and stored at `ActionFrameData+0x60`.

Therefore the project-owned `enemy_actions_wbg_prefix::ActionFrameRecord::first_string` is the recovered source for native `ActionFrameData+0x60`.

## Resource path selection

The `updateData()` tail reached through `updateActionFrameMoveValue()+0xda` uses the selected frame's `+0x60` string together with system words `+0x250` and `+0x268` around `0x2ac3a8..0x2ac440`.

The recovered formats are exact native string literals:

- `+0x250 == 0`: `enemy%02d/res/%s`;
- `+0x250 == 1`: `npc%02d/res/%s`;
- `+0x250 == 2`: `pet%02d/res/%s`.

The `%02d` value comes from system `+0x268`; `%s` is `ActionFrameData+0x60`. `%02d` is a minimum field width and does not truncate wider decimal values.

For other `+0x250` values, this block does not construct one of the three recovered formatted paths.

## `looping` special case

After path selection native compares `ActionFrameData+0x60` with the literal `looping`.

When equal, it passes the `+0x60` string itself directly to `CCSpriteFrameCache::addSpriteFramesWithFile(...)` and then leaves this resource-loading sub-block. The formatted-path `recordEnemyObjectRes(...)` operation is not taken on this special path.

For non-`looping` strings, the observed formatted-path load is gated by unsigned `(system+0x250 - 1) <= 1`, so only `+0x250 == 1` or `2` perform both:

1. `CCSpriteFrameCache::addSpriteFramesWithFile(formatted_path)`;
2. `BattleManager::recordEnemyObjectRes(formatted_path)`.

The `+0x250 == 0` branch still constructs its `enemy%02d/res/%s` string, but this particular add/record block is not entered for the ordinary non-`looping` case.

## Reconstruction

`enemy_actions_runtime_state::State` retains offset-named `field_250` and `field_268`. `apply_update_data_resource_selection(document, state)`:

- reuses the proven `updateData()` readiness/current-frame entry gate;
- obtains `ActionFrameRecord::first_string` as the `+0x60` resource string;
- reproduces the three recovered path formats;
- reports the exact observed sprite-frame-cache and resource-record requests without performing engine IO;
- preserves state unchanged.

Gameplay meanings for `+0x250` and `+0x268` remain deliberately unnamed despite their visible use in path construction.

## Outside scope

This helper does not reconstruct later sprite-frame lookup, object selection, position/movement, visibility, hit-state, or rendering writes in the large `updateActionFrameMoveValue()` body. Those remain separate bounded targets.

No proprietary payload or decompiler-derived source is included.
