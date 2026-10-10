# EnemyActions secondary sprite base update

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native range

The bounded secondary-sprite property slice is `0x2ac86e..0x2ac984`, after successful selection of a Section-B record from `EnemyActionsData+0x8c`.

## Frame lookup and sprite target

Native resolves the frame name from the current **primary** `ActionFrameData+0x68`, not the selected secondary record. It calls `CCSpriteFrameCache::spriteFrameByName()` and forwards the returned pointer to `setDisplayFrame()` on sprite `system+0x114` without a local null check.

The original `CCSprite` vtable identifies the observed object-vptr slots exactly:

- `+0x1f0` -> `setDisplayFrame(CCSpriteFrame*)`;
- `+0x3c` -> `setScaleX(float)`;
- `+0x44` -> `setScaleY(float)`;
- `+0x54` -> `setPosition(CCPoint const&)`;
- `+0x6c` -> `setAnchorPoint(CCPoint const&)`;
- `+0x88` -> `setRotation(float)`;
- `+0x180` -> `setOpacity(unsigned char)`;
- `+0x198` -> `setColor(ccColor3B const&)`.

## Color / opacity gate

Native compares `system+0x278` against zero. Only a strictly positive ordered value takes the bright path:

- `field_278 > 0`: color `(255,255,255)`, opacity `30`;
- zero, negative, or unordered/NaN: color `(0,0,0)`, opacity `150`.

## Scale

Using the proven Section-A mapping:

- scale X = primary `AFD+0x44`;
- scale Y = primary `AFD+0x48 * 0.2f`.

Literal-pool words at the native site decode to `0.2f` (`0x3e4ccccd`) and `0.8f` (`0x3f4ccccd`).

## Position

The selected Section-B record contributes its second serialized float, stored at secondary `AFD+0x18`.

Native computes:

- `x = system+0x194 + primary AFD+0x14`;
- `y = system+0x16c + 0.8*(system+0x170) - system+0x198 - 0.2*(system+0x194) + system+0x2a8 + secondary AFD+0x18`.

The resulting point is also stored at `system+0x1c8` before being passed to `setPosition()` on sprite `system+0x114`.

## Remaining properties

Native then applies:

- rotation = primary `AFD+0x34` directly, with no local one-step 360 subtraction;
- anchor = `(primary AFD+0x24, primary AFD+0x28)`;
- `CCSprite::setFlipY(true)` unconditionally.

Finally byte `system+0x16a` gates an additional geometry correction beginning at `0x2ac986`. That follow-up branch is deliberately outside this base contract.

## Project-owned reconstruction

`enemy_actions_secondary_sprite_update.h` composes the already recovered secondary-entry result with the current primary frame and offset-level system inputs. It produces a side-effect plan only and performs no cocos2d calls.

No proprietary payload or decompiler-derived source is included.
