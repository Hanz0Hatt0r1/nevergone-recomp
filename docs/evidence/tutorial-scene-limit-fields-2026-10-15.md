# Tutorial scene-limit field mapping — 2026-10-15

Target: original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

This note closes the raw field-offset and vertical-branch-order portion of the
tutorial spawn geometry. It does **not** claim that the clean-room runtime can
yet reproduce the current values stored in those original object fields.

## Function boundary

The shipped dynamic symbol table exposes `LessonLayer::nextLesson()` at Thumb
symbol value `0x003e2071` (aligned code entry `0x003e2070`). The Attack-wave
branch follows the current scene-limit owner through:

```text
GameScene::sharedGameScene()
  -> GameScene +0x238
  -> dereference +0x20
  -> scene-limit owner
```

The bounded Thumb signature beginning at `0x003e2156` verifies the two loads.

## Right-side X field

At `0x003e2172`, `LessonLayer::nextLesson()` loads unsigned field `+0x4b4` from
the scene-limit owner. The value is converted to float and participates in the
same side-selection expression as the recovered `500.0f` literal stored at
`0x003e2428`.

Therefore the raw field mapping used by `tutorial_enemy_spawn_plan` is:

```text
right_edge_x <- owner +0x4b4
```

## Vertical field order

The first vertical proximity test loads owner field `+0x4b0` at `0x003e21c2`,
converts it to float, subtracts the player's current Y, takes absolute value,
and compares against the recovered `20.0f` threshold loaded at `0x003e2128`.

Only when that first comparison does not select its branch does execution reach
the second vertical test. That test loads owner field `+0x4ac` at `0x003e2224`
and performs the same distance-to-player-Y comparison against `20.0f`.

The mirrored side path independently repeats the same ordering:

- first vertical load: `+0x4b0` at `0x003e22cc`;
- second vertical load: `+0x4ac` at `0x003e233e`.

The existing clean-room `wave_vertical_offsets()` contract checks
`vertical_edge_b` first and `vertical_edge_a` second. The exact original branch
order therefore resolves the names as:

```text
vertical_edge_b <- owner +0x4b0
vertical_edge_a <- owner +0x4ac
right_edge_x    <- owner +0x4b4
```

This closes the previously unresolved A/B ordering without using field-address
ordering or geometric plausibility.

## Reproducible verifier

Run the exact-binary verifier against either the original APK or an extracted
ARMv7 library:

```bash
python3 tools/tutorial_scene_limit_binary_evidence.py \
  /path/to/com.hippiegame.nevergone.apk \
  --output build/tutorial-scene-limit-evidence/tutorial-scene-limit-armv7.txt
```

The verifier requires both the known library SHA-256 and all bounded Thumb
signatures to match. A different build or any mismatched signature fails
closed.

## Remaining production boundary

The field offsets and order are now reviewed, but `ReviewedSceneLimits` needs
the **current values** corresponding to those fields for the selected imported
scene. The clean-room runtime does not hold the original `GameScene` object, so
it must not read hard-coded values or infer them from screenshots.

The next reconstruction step is to recover where the original owner fields
`+0x4ac`, `+0x4b0`, and `+0x4b4` are populated, map those sources to already
parsed `cp_lesson.glData` data when structurally proven, and only then produce a
generation-stamped `ReviewedSceneLimits` value provider.
