# SingleSelectHero native rune input

This increment moves career selection itself from the temporary Android compatibility controls onto the reconstructed GLES SingleSelectHero scene. It intentionally stops at evidence-backed rune hit testing and gesture dispatch; confirm-button routing, OpenTheDoor/Carousel execution, and pressed/glow rendering remain separate follow-up boundaries.

## Evidence-backed hit boxes

`SingleSelectHero::initUI()` creates five `CCMenuItemSprite` rune controls tagged `1..5` at the recovered centers:

```text
1 -> (568, 495)
2 -> (568, 405)
3 -> (568, 315)
4 -> (568, 225)
5 -> (568, 135)
```

Each normal rune is imported from `xrfuwenNN.png`. TexturePacker may trim transparent pixels, so the visible crop (`width/height`, `left/top`) is not the menu-item hit area. The Cocos sprite content size is the untrimmed TexturePacker `sourceWidth/sourceHeight`.

`single_select_hero_rune_layout::content_rect_for_surface()` therefore:

1. centers the full imported normal-frame source size on the recovered rune center;
2. uses the existing 1136x640 aspect-fit transform and viewport offsets;
3. produces the exact surface-pixel rectangle used for touch hit testing.

Rendering continues to use the trimmed visible rectangle through `quad_for_surface()`.

## Gesture contract

`single_select_hero_touch_state` is a thread-safe single-pointer gesture boundary shared by the Android UI thread and native state:

- DOWN / POINTER_DOWN inside a rune captures that pointer and tag;
- another pointer cannot steal the gesture;
- MOVE keeps the original tag armed while recording whether the pointer is still inside its content box;
- UP / POINTER_UP activates only when released inside the same rune;
- release outside cancels activation;
- CANCEL / OUTSIDE clears the capture.

The state is deliberately renderer-independent so the loaded glow frame can consume its `inside` state in the next visual increment.

## Native dispatch

The existing Java touch path already gives `nativeOnServerSelectionTouch(...)` first refusal before generic scene input. `server_selection_jni.cpp` now delegates to `single_select_hero_touch_bridge` first; the fresh-role route is mutually exclusive with server/management routes, so no new Java MotionEvent router is introduced.

A successful rune release dispatches the exact tag/career through:

```text
single_select_hero_touch_bridge
  -> fresh_role_compat_state::select_career(tag)
  -> single_select_hero_state::select_career(tag)
```

The compatibility layer is still used only for its explicit temporary OpenTheDoor/Carousel transition collapse. No career remapping is introduced.

The bridge refuses input while CharacterNameLayer is active even though `single_select_hero_state` itself remains alive underneath it; the top semantic `fresh_role_compat_state::Mode` must be `kCareerSelection`.

## Asset and surface-size staging

`SingleSelectHeroBaseComposer` stages the normal rune's imported frame geometry into the native touch bridge while it already extracts the atlas for the renderer. No proprietary pixels are duplicated or stored in the repository.

Until the temporary compatibility surface is removed, `FreshRoleCompatOverlay` publishes the actual `GameSurfaceView` width/height to native code from the Android view tree. Touch coordinates and those dimensions are therefore in the same local surface coordinate system. This publisher is transitional plumbing, not reconstructed game behavior.

## Tests

`single_select_hero_rune_layout_smoke` now verifies:

- full source/content hit rectangles versus trimmed visible rectangles;
- boundary containment;
- exact recovered centers;
- aspect-fit scaling and wide-surface letterbox offsets;
- invalid geometry/surface rejection.

`single_select_hero_touch_state_smoke` verifies pointer capture, second-pointer rejection, exit/re-entry, release-inside activation, release-outside cancellation, and CANCEL semantics.

Fast CI runs both host smokes. The main-branch full Android lane remains the JNI/CMake/Gradle/NDK and 16 KiB compatibility gate.

## Remaining boundary

This increment does not claim the full shipped interaction is restored. The next native steps are:

1. draw `xrfuwenfaguangNN.png` while the captured pointer is inside the armed rune;
2. reconstruct and route the SingleSelectHero Confirm button from evidence-backed geometry;
3. execute recovered OpenTheDoor/Carousel timing instead of the temporary immediate transition collapse;
4. once those are in place, remove career-selection controls and surface-size publishing from `FreshRoleCompatOverlay`.
