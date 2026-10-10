# EnemyActions armor resource selection

Original target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This slice starts after a Section-C armor record has matched `ActionFrameData+0x74 == system+0x294`.

## Section-C `+0x60`

The Section-C constructor path around `0x28fb5a..0x28fc3e` creates an `ActionFrameData`, stores the serialized first `int32` at `AFD+0x74`, and stores the first serialized string as a retained `CCString*` at `AFD+0x60` with its length at `+0x64`.

The armor resource path reads that `AFD+0x60` string.

## Formatted paths

At `0x2acaec..0x2acb68`, native branches on `system+0x250`:

- mode 0: `enemy%02d/res/%s` using `system+0x268` and `AFD+0x60`;
- mode 1: `npc%02d/res/%s`;
- mode 2: `pet%02d/res/%s`;
- mode 3, armor id 0: `l%02d/w_res/L%02d_W%02d_default.plist`, using `+0x268`, `+0x268`, and `+0x188`;
- mode 3, armor id 1: `l%02d/res/%s`;
- mode 3, armor id >=2: this local slice does not assign a new deterministic formatted-path value before the later load block. The reconstruction therefore reports unresolved register provenance instead of inventing a path.

`%02d` is minimum width formatting; values are not truncated to two digits.

## Direct special plist

The literal compared against `AFD+0x60` at `0x2acb6a..0x2acb88` is exactly:

`T_E01_default.plist`

On equality, native calls `CCSpriteFrameCache::addSpriteFramesWithFile(AFD+0x60)` directly and skips the ordinary formatted-path resource-recording block. No `BattleManager::recordEnemyObjectRes()` call is made on this direct path.

## Ordinary load gates

For non-special records, native reaches the ordinary formatted-path add/record block under these recovered conditions:

- mode 0: unless `system+0x260` is one of `0x16`, `0x17`, `0x1a`, `0x22`;
- mode 1: unless `system+0x260` is `0x16` or `0x17`;
- modes 2 and 3: the control-flow reaches the block unconditionally;
- values outside 0..3: skip the ordinary block.

When both the control-flow reaches that block and the formatted path is deterministically recovered, native performs:

1. `CCSpriteFrameCache::addSpriteFramesWithFile(formatted_path)`;
2. `BattleManager::recordEnemyObjectRes(formatted_path)`.

For mode 3 with armor id >=2, control-flow still reaches the block but the source register for the path is not assigned in the bounded local slice. The helper intentionally emits no deterministic IO request for that unresolved case.

## Deliberate boundary

This helper reports resource requests only. It does not reconstruct the subsequent armor position/frame/scale/rotation/anchor/visibility updates beginning at `0x2acbf6`.

No proprietary payload or decompiler-derived source is committed.
