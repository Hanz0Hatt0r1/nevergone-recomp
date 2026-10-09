# SingleSelectHero career-rune presentation

This note records the clean-room reconstruction of the shipped `SingleSelectHero::initUI()` career-rune menu. Original atlas bytes and disassembly are not stored in the repository; the renderer extracts frames only from the user-imported original assets at runtime.

## Recovered menu construction

`initUI()` creates five tagged `CCMenuItemSprite` entries. For tag `NN` it formats `xrfuwenNN.png` and `xrfuwenfaguangNN.png`. The first frame is normal and the `faguang` frame is selected/pressed; it is not a persistent current-career indicator.

The construction loop assigns sender tags `1..5`. All five are valid career controls.

## Exact positions

All five items use `x = visibleWidth * 0.5` and `y = visibleHeight - 55 - 90 * tag`. On the recovered 1136x640 design surface the centers are `(568,495)`, `(568,405)`, `(568,315)`, `(568,225)`, and `(568,135)`.

`single_select_hero_rune_layout` maps each imported TexturePacker frame by its untrimmed source rectangle and trim offset through the shared centered aspect-fit transform. All five tags render at full opacity.

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

The layout helper exposes source-size-based `hit_rect_for_surface()` and `hit_test()` using the same centered aspect-fit transform as rendering. Host coverage checks all five recovered rectangles, boundary inclusion/exclusion, invalid inputs, and a 2x surface.

## Pressed frame behavior

The native rune input bridge now drives the staged glow frame directly:

- DOWN inside a rune publishes that tag as the presentation-only pressed rune;
- MOVE outside clears the presentation tag and MOVE back inside restores it;
- UP, POINTER_UP, CANCEL, locked input and reset clear it;
- while the tag is present, `frame_index(tag, false)` resolves to the matching odd-numbered glow frame;
- after the gesture ends, the renderer immediately returns to the normal frame even if that career became selected.

The published presentation tag is atomic because Android touch dispatch and GLES drawing can run on different threads. Career selection, transition locking and network/create-role state remain owned by their existing state machines.

## Runtime boundary

`SingleSelectHeroBaseComposer` extracts all ten normal/pressed frames from the imported `Singleselechero.plist/png` atlas; no original image bytes are committed. Rune hit routing, pressed visual feedback and recovered near/far transition dispatch are now connected.

The remaining interaction gap on this scene is the separate shipped `menuConfirm` control. It should be reconstructed from evidence-backed resources/content size/position and routed to the existing `single_select_hero_state::confirm_online()` path rather than approximated through the temporary Android compatibility overlay.
