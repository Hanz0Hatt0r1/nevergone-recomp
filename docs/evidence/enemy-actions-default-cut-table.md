# EnemyActions default cut table

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records the bounded `EnemyActionsSystem::initResDefaultRectArray()` contract that creates the array later read at `system+0x27c` by the recovered `updateActionFrameMoveValue()` path.

## Native construction

`EnemyActionsSystem::initResDefaultRectArray()` starts at `0x2ab62c`.

The trace at `0x2ab632..0x2ab64e` establishes the container contract:

- read `system+0x27c`;
- when null, create `CCArray` with requested capacity `0x80`;
- store it back to `system+0x27c` and retain it;
- remove all previous objects before rebuilding the table.

The loop at `0x2ab664..0x2ab74c` walks every primary `ActionFrameData` in `EnemyActionsData+0x88` in array order. The resource preload portion uses the already recovered `AFD+0x60` string and `system+0x250/+0x268` path selection before the frame lookup.

For every primary frame, `0x2ab71e..0x2ab748` performs the table materialization:

1. obtain `CCSpriteFrameCache::sharedSpriteFrameCache()`;
2. read selected `ActionFrameData+0x68` and call `getCString()`;
3. call `CCSpriteFrameCache::spriteFrameByName()` with that key;
4. create an `ActionsCut` object;
5. copy the `ActionFrameData+0x68` `CCString*` into `ActionsCut+0x14`;
6. read word/float storage at returned `CCSpriteFrame+0x44` and store it at `ActionsCut+0x1c`;
7. append the `ActionsCut` to `system+0x27c`.

Cocos2d `CCRect` storage in this binary places the fourth float at frame offset `+0x44`; the later recovered code copies a frame rect from `CCSpriteFrame+0x38` and overwrites that fourth float with `ActionsCut+0x1c`. The project layer therefore records this value conservatively as the observed sprite-frame rect height rather than assigning broader gameplay meaning.

Native does not show a null check between `spriteFrameByName()` and the `CCSpriteFrame+0x44` read. The clean-room helper does not reproduce a null dereference: unresolved lookup observations stop construction and report the primary index where materialization could not continue.

## Later consumer

In `updateActionFrameMoveValue()` the fallback path at `0x2ac4e6..0x2ac518` scans `system+0x27c` from index zero. Each candidate loads `ActionsCut+0x14` and compares it with current `ActionFrameData+0x68`. A match stops the scan; duplicate keys therefore select the first table entry.

This is reconstructed by `enemy_actions_default_cut_table::first_match()`.

## Project-owned representation

`enemy_actions_default_cut_table.h` exposes:

- `kNativeRequestedCapacity == 0x80`;
- `SpriteFrameObservation { resolved, rect_height }` as the external cache observation needed to materialize one entry;
- `Entry { primary_index, frame_name_14, frame_name_length_6c, rect_height_1c }`;
- `build(document, observations)` preserving primary frame order;
- `first_match(entries, frame_name_68)` preserving the native first-match scan.

`frame_name_length_6c` is retained alongside the copied string because Section-A construction already proves the selected frame key originates from the third serialized string and its signed serialized length at `ActionFrameData+0x6c`; the native `ActionsCut` itself stores only the string pointer and rect-height value.

## Outside scope

This contract does not yet reconstruct:

- the separate `system+0x278 > 0` search through `EnemyObject::getEnemyActionCutObjectArray()`;
- the sprite-frame rect mutation performed after a cut match;
- `ActionsCut+0x18` writes on the enemy-object cut path;
- movement, position, visibility or rendering writes after the cut-selection block;
- a real cocos2d frame-cache implementation.

No proprietary payload or decompiler-derived source is included.
