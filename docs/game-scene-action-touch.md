# GameScene A/B/C touch bridge

## Scope

This reconstruction connects Android `MotionEvent` input to the recovered NeverGone `ControlsButton` geometry and renders the imported original A/B/C artwork without hard-coded button sizes.

The shipped defaults and hit-test rules are documented in `docs/evidence/controls-action-buttons-2026-10-12.md`:

- A: `ab.png / ab_s.png`, center `(visibleWidth - 238, 90)`, `CONTROLSSTATE 7`;
- B: `db.png / db_s.png`, center `(visibleWidth - 96, 90)`, `CONTROLSSTATE 8`;
- C: `jb.png / jb_s.png`, center `(visibleWidth - 136, 224)`, `CONTROLSSTATE 9`;
- reject the left half before testing the normal sprite's `boundingBox()`.

## Runtime assets

`GameSceneActionAssetLoader` resolves all six action sprite basenames through `GameSceneAtlasResolver.resolveResource()`, which mirrors the recovered `CCFileUtils` search-path order including `controls_ui`.

The loader decodes user-imported normal and selected images to ARGB8888 on the GL render thread. Normal sprite dimensions are also uploaded to `game_scene_action_surface_input` and remain the sole hit-box source. Missing/corrupt assets clear both textures and bounds, so the route fails closed rather than inventing dimensions or artwork.

No proprietary resource bytes are committed to the repository.

## Rendering

`game_scene_action_compositor` owns GLES2 textures for normal/selected A/B/C states and draws them at the recovered default centers in 1136x640 design space. It is visible only while the offline startup route is `game-levels`; the tutorial loading compositor still draws last and therefore owns the complete loading surface.

The selected `*_s.png` texture is chosen from live pointer-capture state. With multiple pointers, a button remains selected until the last pointer captured by that control is released/cancelled. GLES context recreation clears the texture objects and forces one asset restage even if the fresh-role generation has not changed.

## Android coordinate mapping

`game_scene_action_surface_input` maps Android top-origin surface coordinates into the same 1136x640 Cocos design space used by the reconstructed joystick:

```text
designX = surfaceX * 1136 / surfaceWidth
designY = 640 - surfaceY * 640 / surfaceHeight
```

The recovered layout classifier derives each centered sprite rectangle from its imported normal width/height.

## Pointer lifecycle

The bridge keeps independent pointer captures so joystick + action-button and multiple right-side pointers do not require a single global touch owner.

- DOWN / POINTER_DOWN inside A/B/C -> semantic `Press` and selected texture;
- MOVE of a captured pointer remains captured;
- UP / POINTER_UP -> semantic `Release` and normal texture when no pointer remains;
- release inside the same original sprite bounds additionally emits semantic `Tap`;
- release outside the original bounds does not manufacture a tap;
- CANCEL releases all captured action controls and clears selected visuals.

The existing semantic tutorial bridge therefore turns a real on-screen C tap into `Jump -> Attack`. Attack/Block/Counter/Dodge/Air-Attack remain objective-gated and are not completed merely by raw button input.

## JNI routing

`post_scene_overlay_jni` dynamically wraps `GameSurfaceView.nativeOnTouch` and:

1. offers touches to the action-button surface bridge only while the offline startup route is `game-levels`;
2. returns immediately when an action control owns the pointer;
3. forwards all other events to the pre-existing exported `GameSurfaceView_nativeOnTouch`, preserving joystick and earlier UI behavior.

Action textures are staged once per fresh-role handoff during the tutorial-loading/game-levels boundary, with an explicit restage after GL context recreation.

## Remaining boundary

A/B/C geometry, normal/selected visuals, semantic `CONTROLSSTATE` identity, and the recovered base A/B action requests are live. A requests use the original walk/run variants when those committed player states are active; C remains tied to the tutorial's completed jump transition. Remaining work includes synthesized Controls function states, the guarded B alternatives outside the initial baseline, block/combo branches, and the corresponding enemy damage outcomes.
