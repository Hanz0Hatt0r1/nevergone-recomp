# GameScene A/B/C touch bridge

## Scope

This reconstruction connects Android `MotionEvent` input to the recovered NeverGone `ControlsButton` geometry without hard-coded button sizes.

The shipped defaults and hit-test rules are documented in `docs/evidence/controls-action-buttons-2026-10-12.md`:

- A: `ab.png`, center `(visibleWidth - 238, 90)`, `CONTROLSSTATE 7`;
- B: `db.png`, center `(visibleWidth - 96, 90)`, `CONTROLSSTATE 8`;
- C: `jb.png`, center `(visibleWidth - 136, 224)`, `CONTROLSSTATE 9`;
- reject the left half before testing the normal sprite's `boundingBox()`.

## Runtime asset dimensions

`GameSceneActionAssetLoader` resolves the three normal sprite basenames through `GameSceneAtlasResolver.resolveResource()`, which already mirrors the recovered `CCFileUtils` search-path order including `controls_ui`.

The loader uses `BitmapFactory.Options.inJustDecodeBounds`; it does not decode or retain proprietary pixels. Only the original width and height are uploaded to `game_scene_action_surface_input`. Invalid/missing dimensions fail closed and therefore create no touch target.

## Android coordinate mapping

`game_scene_action_surface_input` maps Android top-origin surface coordinates into the same 1136x640 Cocos design space used by the reconstructed joystick:

```text
designX = surfaceX * 1136 / surfaceWidth
designY = 640 - surfaceY * 640 / surfaceHeight
```

The recovered layout classifier then derives each centered sprite rectangle from its imported width/height.

## Pointer lifecycle

The bridge keeps independent pointer captures so joystick + action-button and multiple right-side pointers do not require a single global touch owner.

- DOWN / POINTER_DOWN inside A/B/C -> semantic `Press`;
- MOVE of a captured pointer remains captured;
- UP / POINTER_UP -> semantic `Release`;
- release inside the same original sprite bounds additionally emits semantic `Tap`;
- release outside the original bounds does not manufacture a tap;
- CANCEL releases all captured action controls.

The existing semantic tutorial bridge therefore turns a real on-screen C tap into `Jump -> Attack`. Attack/Block/Counter/Dodge/Air-Attack remain objective-gated and are not completed merely by raw button input.

## JNI routing

`post_scene_overlay_jni` already owns dynamic `GameSurfaceView` registration. It registers a `nativeOnTouch` wrapper that:

1. offers touches to the action-button surface bridge only while the offline startup route is `game-levels`;
2. returns immediately when an action control owns the pointer;
3. forwards all other events to the pre-existing exported `GameSurfaceView_nativeOnTouch`, preserving joystick and earlier UI behavior.

Action sprite dimensions are staged once per fresh-role handoff during the tutorial-loading/game-levels boundary. No resource bytes are committed to this repository.

## Remaining boundary

This bridge proves **which control was touched** and publishes the recovered A/B/C `CONTROLSSTATE` identity. It does not yet reconstruct the complete downstream `Controls::OnHandleButton -> MEPlayer` combat action dispatch or render the A/B/C sprites. Those are separate next steps.
