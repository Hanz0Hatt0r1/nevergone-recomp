# EnemyActions common base sprite update

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note records the common sprite-property slice at `0x2ac600..0x2ac6e8` after the recovered cut-processing branches rejoin.

## Virtual calls identified from the original `CCSprite` vtable

The original binary exports `vtable for cocos2d::CCSprite` at `0x9263d0`. Accounting for the Itanium ABI vptr address point, the object-vtable slots used by this block resolve to:

- `+0x1f0` -> `CCSprite::setDisplayFrame(CCSpriteFrame*)`;
- `+0x54` -> `CCSprite::setPosition(CCPoint const&)`;
- `+0x6c` -> `CCSprite::setAnchorPoint(CCPoint const&)`;
- `+0x3c` -> `CCSprite::setScaleX(float)`;
- `+0x44` -> `CCSprite::setScaleY(float)`;
- `+0x88` -> `CCSprite::setRotation(float)`;
- `+0x80` -> `CCSprite::setVisible(bool)`;
- `+0x180` -> `CCSprite::setOpacity(unsigned char)`.

`setFlipX(bool)` is then called directly at `0x2ac6e0..0x2ac6e8`.

This removes the need to infer method identities from argument shapes.

## Section-A offsets

The native Section-A constructor at `0x28f77c..0x28f818` establishes the serialized-float mapping consumed here:

- serialized float 0 -> `ActionFrameData+0x14`;
- float 1 -> `+0x18`;
- float 2 -> `+0x24`;
- float 3 -> `+0x28`;
- float 9 -> `+0x34`;
- float 10 -> `+0x44`;
- float 11 -> `+0x48`;
- serialized bool -> byte `+0x40`;
- serialized second int32 -> word `+0x50`;
- third string -> `+0x68`.

Native reads only the low byte of `ActionFrameData+0x50` for the opacity call. This also matches the genuine shipped records whose second int32 is `255`.

## Common property sequence

At `0x2ac600`, native calls `setDisplayFrame()` on sprite `system+0x10c` with the previously resolved frame pointer. This call is made even on paths where that pointer can be null.

The position written both to temporary/system point storage at `system+0x1c0` and to the sprite is:

- `x = AFD+0x14 + system+0x194`;
- `y = AFD+0x18 + system+0x198 + cut_offset_18 - system+0x280`.

`cut_offset_18` is the selected/new EnemyObject-owned `ActionsCut+0x18`. Default-cut and no-cut paths explicitly enter the common block with zero in the corresponding VFP register.

Native then applies:

- anchor point = `(AFD+0x24, AFD+0x28)`;
- scale X = `AFD+0x44`;
- scale Y = `AFD+0x48`;
- rotation from `AFD+0x34`;
- visible = `true`;
- opacity = low byte of `AFD+0x50`;
- flip X = byte `AFD+0x40`, XOR/inverted when system byte `+0x16a` is nonzero.

### Rotation behavior

The rotation path compares `AFD+0x34` against literal `360.0f` and performs exactly one subtraction when the value is greater than or equal to 360. It is not modulo arithmetic:

- `450 -> 90`;
- `810 -> 450`;
- values below 360 pass through unchanged;
- unordered/NaN does not take the GE conditional subtraction.

## Project-owned reconstruction

`enemy_actions_base_sprite_update.h` exposes a side-effect plan built directly from the parsed `ActionFrameRecord`, the external sprite-frame resolution observation, a cut `+0x18` offset, and the three recovered system inputs `+0x194/+0x198/+0x280` plus byte `+0x16a`.

The plan reports the exact arguments for display-frame, position, anchor, scale, rotation, visibility, opacity and flip-X calls, plus the reconstructed `system+0x1c0` point value. It performs no cocos2d calls.

## Outside scope

When resulting flip-X is true, native enters an additional geometry/position correction block beginning at `0x2ac6f0`. That path is deliberately excluded here. Later secondary sprites and armor-array processing are also outside this contract.

No proprietary payload or decompiler-derived source is included.
