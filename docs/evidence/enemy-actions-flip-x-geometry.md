# EnemyActions flip-X geometry correction

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records the flip-X-only geometry slice at `0x2ac6f0..0x2ac7d2`, immediately after the common base-sprite update.

## Entry condition

At `0x2ac6ea..0x2ac6ee`, native tests the final flip-X value already passed to `CCSprite::setFlipX(bool)`. If it is zero, control jumps directly to `0x2ac7d4`; none of the geometry below runs.

The final flip-X value already includes the recovered `system+0x16a` inversion from the common base-sprite contract.

## Position correction

The block reconstructs the same common position point first, then forms a pivot from:

- `ActionFrameData+0x1c`;
- `ActionFrameData+0x20`.

Section-A construction maps these to serialized float indices 6 and 7.

The two small point helpers called at `0x2ac742` and `0x2ac794` operate as point subtraction and point addition respectively. Between those calls native negates the Y component of the intermediate point. Therefore the exact sequence is:

```
delta = pivot - current_position
delta.y = -delta.y
mirrored = pivot + delta
```

Algebraically:

- `mirrored.x = 2 * pivot.x - current_position.x`;
- `mirrored.y = current_position.y`.

The result is copied back to system point storage `+0x1c0` and passed to `CCSprite::setPosition()`.

## Anchor, rotation and scale

While in the flip-X branch native also reapplies:

- anchor X = `0.5 - (AFD+0x24 - 0.5)` = `1.0 - AFD+0x24`;
- anchor Y = `AFD+0x28`;
- rotation = negative of the already one-step-normalized rotation held from the common path;
- scale X = `AFD+0x44`;
- scale Y = `AFD+0x48`.

The scale values are not negated. No extra clamp or modulo is observed.

At `0x2ac7d4` native leaves this branch and reapplies opacity before moving to the next update slice; that later instruction is outside this helper because opacity is already covered by the common base-sprite contract.

## Project-owned reconstruction

`enemy_actions_flip_x_geometry.h` consumes the parsed Section-A frame and the already-built common base-sprite plan. It returns a deterministic side-effect plan containing the corrected position/system `+0x1c0`, mirrored anchor, negated rotation and unchanged scale values.

The helper performs no cocos2d calls and is a no-op when the final base-plan flip-X value is false.

No proprietary payload or decompiler-derived source is included.
