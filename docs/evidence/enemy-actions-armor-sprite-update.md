# EnemyActions armor sprite update

Original target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note covers the selected armor record's sprite update at `0x2acbf6..0x2acde2`.

## Section-C field mapping used here

The Section-C constructor path around `0x28fb5a..0x28fc38` maps serialized values into `ActionFrameData` as follows:

- float[0] -> `+0x14`
- float[1] -> `+0x18`
- float[6] -> `+0x1c`
- float[7] -> `+0x20`
- float[2] -> `+0x24`
- float[3] -> `+0x28`
- float[9] -> `+0x34`
- float[10] -> `+0x44`
- float[11] -> `+0x48`
- first bool -> `+0x40`
- second int32 -> `+0x50`
- third serialized string -> `+0x78`.

## Base sprite update

For armor id `i`, native operates on sprite pointer `system+0x130+4*i` and stores the working point at `system+0x1d0+8*i`.

The base point is:

`x = system+0x194 + AFD+0x14`

`y = system+0x198 + AFD+0x18 - system+0x280`

Native then:

1. looks up `AFD+0x78` through `CCSpriteFrameCache::spriteFrameByName()`;
2. forwards the result to `setDisplayFrame()` without a local null check;
3. sets the base position;
4. sets anchor to `AFD+0x24/+0x28`;
5. sets scale X/Y from `AFD+0x44/+0x48`;
6. sets rotation from raw `AFD+0x34`;
7. sets visibility to `(system+0x258+i) XOR 1`;
8. computes flipX from `AFD+0x40`, additionally inverted when `system+0x16a != 0`;
9. applies `setFlipX()`.

## Flip-X geometry

When the resulting flipX value is nonzero, native mirrors the base point around pivot `AFD+0x1c/+0x20` using the recovered CCPoint subtract/add helpers. The Y component is explicitly negated between subtract and add, so the final Y returns to the base Y while X is mirrored:

`final_x = 2 * pivot_x - base_x`

`final_y = base_y`

Anchor X becomes `1 - anchor_x`; anchor Y is unchanged.

Rotation is sign-bit toggled with XOR `0x80000000`, not arithmetic negation. The helper preserves that bit-level behavior for signed zero and NaN payloads.

Scale X/Y are re-applied unchanged after the mirrored position/rotation update.

## Opacity and mode-3 visibility

At `0x2acdbc`, native loads byte `AFD+0x50` and calls the `CCSprite::setOpacity` vtable slot. Therefore only the low byte of the serialized second int32 is observable here.

If `system+0x250 == 3`, native then calls `setVisible(true)` unconditionally, overriding the earlier visibility derived from `system+0x258+i`.

## Following child-order call

At `0x2acde2..0x2acdf0`, native invokes the `EnemyActionsSystem` vtable slot at object-vptr `+0xf0`. Resolving the original vtable (`vtable for EnemyActionsSystem` at `0x8e5410`) shows that this inherited slot is `cocos2d::CCNode::reorderChild(cocos2d::CCNode*, int)` at `0x515e21`.

The call arguments are the armor sprite and `AFD+0x38`. The helper deliberately stops before this call because the exact serialized-float-to-`AFD+0x38` conversion contract, including exceptional float behavior, is being kept as a separate bounded reconstruction.

No proprietary payload or decompiler-derived source is committed.
