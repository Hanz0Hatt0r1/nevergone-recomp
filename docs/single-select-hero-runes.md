# SingleSelectHero career-rune presentation

This note records the clean-room reconstruction of the shipped `SingleSelectHero::initUI()` career-rune menu. Original atlas bytes and disassembly are not stored in the repository; the renderer extracts frames only from the user-imported original assets at runtime.

## Recovered menu construction

`initUI()` creates five tagged `CCMenuItemSprite` entries. For tag `NN` it formats:

```text
xrfuwenNN.png
xrfuwenfaguangNN.png
```

The first frame is the normal image and the `faguang` frame is the selected/pressed image. The recovered loop assigns sender tags `1..5` and does not disable tags `3..5`; all five are valid career controls. The previous opacity-120 treatment of careers 3..5 was based on an incomplete two-career reconstruction and has been removed.

## Exact positions

All five items use `x = visibleWidth * 0.5` and `y = visibleHeight - 55 - 90 * tag`. On the 1136x640 design surface their centers are `(568,495)`, `(568,405)`, `(568,315)`, `(568,225)`, and `(568,135)` for careers 1 through 5.

`single_select_hero_rune_layout` maps each imported TexturePacker frame by its untrimmed source rectangle and trim offset through the centered aspect-fit surface mapping. All five tags now render at full opacity. The staged `xrfuwenfaguang` frames remain reserved for the pressed state rather than persistent selection.

## Runtime boundary

`SingleSelectHeroBaseComposer` extracts all ten frames (normal + pressed for tags 1..5) from the imported `Singleselechero.plist/png` atlas. No original image bytes are committed. The next interaction increment should use these imported frame source sizes to build exact touch rectangles, respect the recovered `OpenTheDoor` transition gate for career changes, and route confirm separately through `menuConfirm` semantics.
