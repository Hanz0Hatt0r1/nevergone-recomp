# EnemyActions new EnemyObject cut materialization

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records the bounded side effects reached when the positive `system+0x278` branch selected the EnemyObject-owned `ActionsCut` array and found no existing entry for current `ActionFrameData+0x68`.

## Native conditions

The unmatched EnemyObject-owned path begins around `0x2ac550` after the ordered array scan has exhausted all entries without a matching `ActionsCut+0x14`.

The path only materializes a new cut when the earlier `spriteFrameByName(ActionFrameData+0x68)` result is non-null. A null sprite frame skips the new-cut allocation/append work and rejoins later downstream processing.

## Rect arithmetic

With a valid sprite frame, native copies its rect and applies the already recovered EnemyObject cut adjustment:

- read system float `+0x280`;
- obtain `EnemyObject::actionWithHPointOffset()`;
- compute the positive difference only when the H-point offset is strictly below `+0x280`;
- replace the fourth copied rect float with `original_rect_height - delta`;
- call `CCSpriteFrame::setRect()`.

This arithmetic is shared through `enemy_actions_cut_rect::adjust_enemy_cut()` rather than duplicated.

## New `ActionsCut`

The trace around `0x2ac5b6..0x2ac5fc` then creates a new `ActionsCut` and initializes the observable fields:

- `ActionsCut+0x14` receives a retained/copy CCString for current `ActionFrameData+0x68`;
- `ActionsCut+0x1c` receives the **original** fourth sprite-frame rect float before the height reduction;
- `ActionsCut+0x18` receives `delta * 0.5f`;
- the new object is appended to `EnemyObject::getEnemyActionCutObjectArray()`.

This order matters: `+0x1c` preserves the original rect height, not the patched height. It is therefore reusable by the existing-cut path on later frames.

## Project-owned reconstruction

`enemy_actions_new_cut.h` exposes a side-effect plan rather than performing cocos2d allocation:

`materialize(unmatched_enemy_cut_path, sprite_frame_resolved, current_frame_name_68, original_rect_height, field_280, action_hpoint_offset)`

When both path and sprite-frame gates are satisfied, the result reports:

- sprite-frame rect patch request;
- `ActionsCut` creation request;
- EnemyObject cut-array append request;
- `frame_name_14`;
- `field_18 = delta / 2`;
- `field_1c = original_rect_height`;
- patched rect height and delta.

If either gate is absent, the helper emits no allocation/append/rect-patch requests.

## Outside scope

This layer does not construct actual CCString/ActionsCut objects, manage cocos2d retain/autorelease ownership, mutate a real CCArray, or model the common downstream sprite transform path.

No proprietary payload or decompiler-derived source is included.
