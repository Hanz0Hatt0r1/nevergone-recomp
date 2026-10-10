# EnemyActions composed sprite update

Target: clean-room reconstruction of original Never Gone 1.0.9 ARMv7 behavior. Original `libcocos2dcpp.so` SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This layer composes two already independently recovered and tested native slices:

1. the common base-sprite update at `0x2ac600..0x2ac6e8`;
2. the conditional flip-X geometry correction at `0x2ac6f0..0x2ac7d2`.

It introduces no new native semantics. Its purpose is to expose the final arguments a future cocos2d adapter must apply after those two proven slices execute in sequence.

## Composition contract

`enemy_actions_sprite_update::build()` first calls the common base planner. That preserves the recovered display-frame request, position/system `+0x1c0`, anchor, scale, one-step rotation normalization, visibility, low-byte opacity and final flip-X value.

It then calls the flip-X geometry planner with that exact base result. When final flip-X is false, the final plan remains the base plan. When final flip-X is true, only the native fields rewritten by the conditional geometry block replace their base values:

- position and system `+0x1c0` point;
- anchor;
- rotation;
- scale (reapplied with the same values).

Display-frame request, visibility, opacity and final flip-X remain those from the common base slice.

## Regression significance

The focused smoke exercises the continuous chain with concrete offset-derived inputs:

- common position `(5, 13)`;
- pivot X `10`;
- resulting mirrored position `(15, 13)`;
- common rotation `450 -> 90` followed by flip geometry `90 -> -90`;
- anchor X `0.25 -> 0.75`;
- low-byte opacity `0x1234 -> 0x34`.

It also verifies that `system+0x16a` inversion can turn the serialized flip byte off, suppressing the conditional geometry while leaving the base update intact.

## Deliberate boundary

The frame-zero bounding-box calculations immediately after this sequence, including writes around `system+0x16c/+0x170`, remain outside this contract because their exact stack/`CCRect` field mapping is not yet committed as evidence.

No proprietary payload or decompiler-derived source is included.
