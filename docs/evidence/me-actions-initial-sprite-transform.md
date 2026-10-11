# MeActions initial primary-sprite transform evidence

Target: clean-room reconstruction of Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note continues the player path recorded in `me-actions-initial-frame.md`. It bounds the final visible transform of the primary `MeActionsSystem` body sprite during the first `MEPlayer::createActions() -> MeActionsSystem::play() -> updateData()` call.

## Primary sprite identity

`MeActionsSystem::initWithFile()` resolves primary frame 0 through `CCSpriteFrameCache::spriteFrameByName()` and creates a `ShaderPostProcessing` sprite with `createWithSpriteFrame()`. The result is stored at:

```text
MeActionsSystem + 0x118
```

At `0x32c0c2..0x32c0d2`, native adds that sprite to the `MeActionsSystem` node. The integer Z argument is `ActionFrameData +0x38`.

During `updateData()`, `0x32a11a..0x32a128` invokes the inherited `CCNode::reorderChild(CCNode*, int)` slot (`object-vptr +0xf0`) with the same body sprite and the current frame's `ActionFrameData +0x38`.

Existing Section-C evidence maps `ActionFrameData +0x38` to serialized float index 8 converted with `VCVT.S32.F32`. Project code rejects non-finite/out-of-range values and uses truncation toward zero for the valid shipped range.

## Initial system constants

The first player action does not require guessed mutable MeActions fields.

`MEPlayer::createActions()` calls `MeActionsSystem::creinitWithFile()` at `0x319092` with the float argument loaded as `0x3f800000` (`1.0f`) at `0x319076`.

`MeActionsSystem::initWithFile()` stores that float at `system +0x2f8` and initializes:

- `+0x204 = 0` at `0x32bdf2`;
- `+0x208 = 0` at `0x32bdf6`;
- `+0x644 = 0` at `0x32be20`;
- `+0x648 = 0` at `0x32be28`;
- byte `+0x1be = 0` at `0x32bd70`.

`MeActionsSystem::play()` reaches `updateData()` without changing those fields. Because `+0x644` is zero, the initial cut/H-point offset path contributes zero.

Therefore the general native formulas reduce to direct ActionFrameData values for the first visible frame.

## Common body-sprite update

The body update begins at `0x329ee4` after the frame pointer has been resolved. The original `CCSprite` vtable identifies the observed virtual slots as:

- `+0x1f0` -> `setDisplayFrame(CCSpriteFrame*)`;
- `+0x54` -> `setPosition(CCPoint const&)`;
- `+0x6c` -> `setAnchorPoint(CCPoint const&)`;
- `+0x3c` -> `setScaleX(float)`;
- `+0x44` -> `setScaleY(float)`;
- `+0x88` -> `setRotation(float)`.

`setFlipX(bool)` is called directly at `0x329fc2`.

For the initial path, before optional flip correction, native applies:

```text
local position = (AFD+0x14, AFD+0x18)
anchor         = (AFD+0x24, AFD+0x28)
scale          = (AFD+0x44, AFD+0x48)
rotation       = AFD+0x34
flipX          = byte(AFD+0x40)
child Z        = int(AFD+0x38)
```

The recovered shared ActionFrameData mapping is:

- `AFD+0x14/+0x18` -> serialized floats 0/1;
- `AFD+0x24/+0x28` -> floats 2/3;
- `AFD+0x1c/+0x20` -> floats 6/7;
- `AFD+0x38` -> float 8 converted to signed int;
- `AFD+0x34` -> float 9;
- `AFD+0x44/+0x48` -> floats 10/11;
- `AFD+0x40` -> serialized bool.

The `+0x1be` facing inversion byte is zero on this initial path, so the serialized bool is the first visible `flipX` value directly.

## Flip-X geometry

When the final flip flag is nonzero, `0x329fce..0x32a104` mirrors the body geometry.

Native rebuilds the current local position, forms a pivot from `AFD+0x1c/+0x20`, computes:

```text
delta = pivot - position
delta.y = -delta.y
mirrored = pivot + delta
```

For finite inputs this is:

```text
mirrored.x = 2 * pivot.x - position.x
mirrored.y = position.y
```

It also changes anchor X to `1.0 - AFD+0x24`, preserving anchor Y.

Unlike the already reconstructed EnemyActions flip path, MeActions has an important extra step: it temporarily applies `-AFD+0x34` before `MeActionsSystem::updateActionFrameMoveValue()` at `0x32a106`, but then `0x32a10c..0x32a118` reapplies the original, non-negated `AFD+0x34` rotation to the body sprite. The final visible rotation is therefore the frame's original rotation even when flipped.

Scale X/Y are never sign-negated; they remain `AFD+0x44/+0x48` on the initial `+0x2f8 = 1.0` path.

## Coordinate space

These body-sprite coordinates are local to the `MeActionsSystem` node. The node itself is positioned from `MEPlayer::getPlayerPos()`, whose first-scene value is the already recovered bootstrap-plus-type-6 result.

The minimal flattened player renderer therefore has two independent proven inputs:

```text
world/player node point
    = MEPlayer(+0x240 bootstrap) + MEPlayer(+0x248 scene offset)

body local transform
    = current ActionFrameData transform described above
```

Anchor/scale/rotation must be applied around the resolved sprite-frame geometry before adding the player-node world position; simply adding local X/Y to world X/Y is not sufficient for final quad generation.

## Project-owned reconstruction

`me_actions_initial_sprite_transform.h` exposes the final first-frame body transform and child Z with fail-closed finite/range validation. It also records the temporary rotation used during `updateActionFrameMoveValue()` separately from the final visible rotation.

`game_scene_initial_player_sprite_request` composes:

```text
MeActions initial state (current=0, previous=-1)
    -> primary frame 0 key
    -> validated sprite-frame-by-name request
    -> initial body local transform + child Z
    -> recovered MEPlayer node position
```

This is the last action-document-side boundary needed before resolving the sprite frame into atlas pixels/quad geometry.

No proprietary payload or decompiler-derived source is included.
