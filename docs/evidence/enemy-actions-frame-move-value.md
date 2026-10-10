# EnemyActions frame move-value state

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native function

`EnemyActionsSystem::updateActionFrameMoveValue()` is exported at `0x2ac28f`; the recovered bounded state transition is `0x2ac28e..0x2ac364`.

The caller in the larger update tail reaches this helper after the frame-zero bounding-box slice.

## Entry gates

Native returns without changing the modeled fields when any of these conditions holds:

- `system+0x17c == system+0x294`;
- `system+0x108` (EnemyActionsData) is null;
- `EnemyActionsData+0x88` (primary array) is null;
- the raw 32-bit value at `system+0x294` is equal to `CCArray::count()`.

The count check is equality only. This contract deliberately does not replace it with a broader `>=` check.

## Bounding point

The helper samples `CCSprite::sprBoundingBox()` for sprite `system+0x10c` and constructs the point:

- `x = rect.origin.x + rect.size.width * 0.5`;
- `y = rect.origin.y`.

This is the bottom-center point of the observed rectangle.

## Frame zero

When `system+0x294 == 0`, native assigns that point directly to the two floats beginning at `system+0x298`, then stores zero/current frame into `system+0x17c`.

No `+0x180/+0x184` accumulation occurs on this path.

## Nonzero frames

For a nonzero current frame, native computes:

- `delta.x = point.x - system+0x298`;
- `delta.y = point.y - system+0x29c`;
- `system+0x180 += delta.x`;
- `system+0x184 += delta.y`.

It then assigns the **delta point itself** to `system+0x298/+0x29c`, not the newly sampled absolute bottom-center point, and finally stores the current frame into `system+0x17c`.

This unusual assignment is preserved literally rather than normalized into a more conventional previous-position tracker.

## Project-owned reconstruction

`enemy_actions_frame_move_value.h` models only these offset-level state transitions from an externally supplied bounding-box observation. It performs no cocos2d calls and assigns no unproven gameplay semantics to the fields.

No proprietary payload or decompiler-derived source is included.
