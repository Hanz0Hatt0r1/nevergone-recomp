# SingleSelectHero career-rune presentation

This note records the clean-room reconstruction of the shipped `SingleSelectHero::initUI()` career-rune menu. Original atlas bytes and disassembly are not stored in the repository; the renderer extracts frames only from the user-imported original assets at runtime.

## Recovered menu construction

`initUI()` creates five tagged `CCMenuItemSprite` entries. For tag `NN` it formats `xrfuwenNN.png` and `xrfuwenfaguangNN.png`. The first frame is normal and the `faguang` frame is selected/pressed; it is not a persistent current-career indicator.

The construction loop assigns sender tags `1..5` and does not disable tags `3..5`. All five are valid career controls. The earlier opacity-120 treatment of careers 3 through 5 came from the superseded two-career model and has been removed.

## Exact positions

All five items use `x = visibleWidth * 0.5` and `y = visibleHeight - 55 - 90 * tag`. On the recovered 1136x640 design surface the centers are `(568,495)`, `(568,405)`, `(568,315)`, `(568,225)`, and `(568,135)`.

`single_select_hero_rune_layout` maps each imported TexturePacker frame by its untrimmed source rectangle and trim offset through the shared centered aspect-fit transform. All five tags render at full opacity. The staged glow frames remain reserved for the pressed state.

## Runtime boundary

`SingleSelectHeroBaseComposer` extracts all ten normal/pressed frames from the imported `Singleselechero.plist/png` atlas; no original image bytes are committed. The next interaction increment should use imported frame source sizes for hit rectangles, respect the recovered `OpenTheDoor` transition gate for career changes, and route confirm separately through `menuConfirm` semantics.
