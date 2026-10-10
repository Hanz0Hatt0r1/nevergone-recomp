# EnemyActions armor z-order / reorderChild

Original target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This contract covers the child-order value used immediately after the armor sprite property update.

## Section-C conversion into `ActionFrameData+0x38`

During Section-C record construction, serialized float index 8 is loaded at `0x28fc22`, converted with `VCVT.S32.F32` at `0x28fc26`, and stored as the 32-bit word at `ActionFrameData+0x38` at `0x28fc2a`.

For finite values whose truncation is representable by signed 32-bit integer, the conversion form used here rounds toward zero. The helper therefore resolves those inputs exactly. NaN, infinities, and out-of-range values remain deliberately unresolved because this bounded reconstruction does not need to invent the architecture's exception-result word.

## Runtime consumer

At `0x2acde2..0x2acdf0`, native loads:

- `this` = `EnemyActionsSystem` / inherited `CCNode`;
- child = armor sprite pointer at `system+0x130+4*armor_id`;
- z-order = `ActionFrameData+0x38`.

The virtual slot at object-vptr `+0xf0` resolves through the original `EnemyActionsSystem` vtable to:

`cocos2d::CCNode::reorderChild(cocos2d::CCNode*, int)`

(symbol address `0x515e21`).

Therefore, whenever the preceding matched armor-record body reaches this point, native requests `reorderChild(armor_sprite, field_38)`.

## Deliberate boundary

The helper reports the reorder request and resolves the z-order only for finite in-range inputs. It performs no cocos2d call and does not cover the next `EnemyActionsData+0x84` root/fixed-tail processing block beginning after the armor loop.

No proprietary payload or decompiler-derived source is committed.
