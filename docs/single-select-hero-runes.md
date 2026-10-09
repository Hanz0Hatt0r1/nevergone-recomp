# SingleSelectHero career-rune presentation

This note records the clean-room reconstruction of the shipped `SingleSelectHero::initUI()` career-rune menu. Original atlas bytes and disassembly are not stored in the repository; the renderer extracts frames only from the user-imported original assets at runtime.

## Recovered menu construction

`initUI()` creates five tagged `CCMenuItemSprite` entries. For tag `NN` it formats `xrfuwenNN.png` and `xrfuwenfaguangNN.png`. The first frame is normal and the `faguang` frame is selected/pressed; it is not a persistent current-career indicator.

The construction loop assigns sender tags `1..5`. All five are valid career controls.

## Exact positions

All five items use `x = visibleWidth * 0.5` and `y = visibleHeight - 55 - 90 * tag`. On the recovered 1136x640 design surface the centers are `(568,495)`, `(568,405)`, `(568,315)`, `(568,225)`, and `(568,135)`.

`single_select_hero_rune_layout` maps each imported TexturePacker frame by its untrimmed source rectangle and trim offset through the shared centered aspect-fit transform. All five tags render at full opacity. The staged glow frames remain reserved for the pressed state.

## Touch rectangles

The visible atlas rectangle and the Cocos menu-item touch rectangle are not always identical. `CCMenuItemSprite` uses the normal sprite's untrimmed content size, while TexturePacker trimming only changes the visible draw quad. The recovered normal source sizes are:

| career | source size | design-space touch rectangle |
| --- | --- | --- |
| 1 | `79 x 99` | `(528.5,95.5)-(607.5,194.5)` |
| 2 | `87 x 89` | `(524.5,190.5)-(611.5,279.5)` |
| 3 | `75 x 95` | `(530.5,277.5)-(605.5,372.5)` |
| 4 | `111 x 110` | `(512.5,360)-(623.5,470)` |
| 5 | `137 x 131` | `(499.5,439.5)-(636.5,570.5)` |

Career 2 demonstrates why this matters: its visible normal frame has a 3-pixel top trim, so drawing begins at design Y `193.5`, while the original menu-item hit rectangle begins at `190.5`.

The layout helper now exposes source-size-based `hit_rect_for_surface()` and `hit_test()` using the same centered aspect-fit transform as rendering. Host coverage checks all five recovered rectangles, boundary inclusion/exclusion, invalid inputs, and a 2x surface.

## Runtime boundary

`SingleSelectHeroBaseComposer` extracts all ten normal/pressed frames from the imported `Singleselechero.plist/png` atlas; no original image bytes are committed. The next interaction increment can route pointer DOWN/UP through these exact hit boxes and show the staged glow frame while pressed. Career changes must still respect the recovered `OpenTheDoor` transition gate; confirm remains a separate `menuConfirm` control.
