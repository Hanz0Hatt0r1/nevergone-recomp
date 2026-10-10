# EnemyActions frame-zero bounding-box state

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native range

The recovered slice is `0x2ac7e4..0x2ac824` inside `EnemyActionsSystem::updateActionFrameMoveValue()`.

At `0x2ac7e4` native loads `system+0x294` and skips the entire slice when it is nonzero. Therefore these writes occur only for current frame zero.

When the frame is zero:

1. `CCSprite::sprBoundingBox()` writes a `CCRect` temporary at `sp+0x28`.
2. `CCNode::boundingBox()` writes a second `CCRect` temporary at `sp+0x38`.
3. Native reads:
   - `sp+0x2c` = sprite bounds `origin.y`;
   - `sp+0x3c` = node bounds `origin.y`;
   - `sp+0x44` = node bounds `size.height`.

The stack mapping follows the normal 16-byte cocos2d `CCRect` layout: point `(x,y)` followed by size `(width,height)`.

## Exact arithmetic

The instruction sequence is:

- `VSUB`: `tmp = spr.origin.y - node.origin.y`;
- `VNMLS`: `tmp = node.height * 0.5 - tmp`;
- `VADD tmp,tmp`: doubles the result;
- store to `system+0x170`;
- subtract that stored value from `spr.origin.y` and store to `system+0x16c`.

Therefore:

- `field_170 = 2 * (node.origin.y + node.height / 2 - spr.origin.y)`;
- `field_16c = spr.origin.y - field_170`.

The project helper preserves the native operation order rather than replacing it with a more algebraically compact expression.

## Scope

`enemy_actions_frame_zero_bounds.h` models only the deterministic float outputs and the `current_frame_294 == 0` gate. It does not invoke cocos2d bounding-box methods itself.

The subsequent recursive `updateActionFrameMoveValue()` call at `0x2ac826` and the secondary-sprite / armor-array logic beginning at `0x2ac82c` remain outside this contract.

No proprietary payload or decompiler-derived source is included.
