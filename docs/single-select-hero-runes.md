# SingleSelectHero career-rune presentation

This note records the clean-room reconstruction of the shipped `SingleSelectHero::initUI()` career-rune menu. Original atlas bytes and disassembly are not stored in the repository; the renderer extracts frames only from the user-imported original assets at runtime.

## Recovered menu construction

`initUI()` creates five tagged `CCMenuItemSprite` entries. For tag `NN` it formats:

```text
xrfuwenNN.png
xrfuwenfaguangNN.png
```

The first frame is the normal image and the `faguang` frame is the menu item's selected/pressed image. It is therefore not treated as a persistent indicator of the currently chosen career.

Only tags `1` and `2` are enabled. Tags `3`, `4`, and `5` remain visible but are disabled and receive opacity `120`.

## Exact recovered positions

All five items share:

```text
x = visibleWidth * 0.5
```

and use:

```text
y = visibleHeight - 55 - 90 * tag
```

On the recovered `1136 x 640` design surface this yields:

| tag | center |
| --- | --- |
| 1 | `(568, 495)` |
| 2 | `(568, 405)` |
| 3 | `(568, 315)` |
| 4 | `(568, 225)` |
| 5 | `(568, 135)` |

`single_select_hero_rune_layout` maps each imported TexturePacker frame by its untrimmed source rectangle and trim offset through the same centered aspect-fit surface mapping used by the rest of the reconstructed screen.

The host smoke explicitly covers the shipped trim of `xrfuwen02.png` (`87 x 83` visible inside an `87 x 89` source with top offset `3`) so the frame is not incorrectly centered from trimmed dimensions.

## Runtime asset lifecycle

`SingleSelectHeroBaseComposer` now extracts all ten frames (normal + pressed for tags 1..5) from the imported `Singleselechero.plist/png` atlas and uploads decoded pixels plus geometry to the native compositor. No original image bytes are committed.

The native layer keeps CPU backing across GLES context recreation, creates textures lazily, and drops scene-owned texture handles while the `OpeningDialogue` route is inactive.

The current renderer draws each rune in its normal state and applies alpha `120/255` to disabled tags 3..5. Pressed-frame selection remains reserved for the input increment, where DOWN/UP routing can drive the exact `CCMenuItemSprite` selected-state behavior without confusing it with persistent career selection.

## Current boundary

This increment restores the visible five-item career-rune menu. It deliberately does not yet bypass the recovered `menuOpenGC` transition gate or invent transition timing. The next interaction increment should connect recovered hit boxes for enabled tags 1/2 to `single_select_hero_state::select_career()` only after the `OpenTheDoor`/Carousel completion timing is represented at runtime, then use the staged glow frames only during the pressed state.
