# SingleSelectHero career-rune presentation

This note records the clean-room reconstruction of the shipped `SingleSelectHero::initUI()` career-rune menu. Original atlas bytes, screenshots, heap dumps and disassembly are not stored in the repository; the renderer extracts frames only from the user-imported original assets at runtime.

## Recovered menu construction

`initUI()` creates five tagged `CCMenuItemSprite` entries. For tag `NN` it formats `xrfuwenNN.png` and `xrfuwenfaguangNN.png`. The construction loop assigns sender tags `1..5`.

Two stable original-game scene-state captures now distinguish construction from runtime presentation:

- all five tagged menu-item objects remain instantiated at the recovered positions;
- only tags `1` and `2` are visible / interactive in the captured shipped state;
- tags `3..5` remain retained state/geometry but are not presented;
- the `faguang` frame is used for the currently selected visible career even after the touch gesture has ended.

A capture with Dawn selected shows persistent glow on tag `1`; a second capture from the same process with Higgs selected shows the glow move to tag `2`. Because the process and ASLR base were unchanged, the same live menu-item structures could be compared directly. The tag-2 selected-image child changes from hidden to visible in the Higgs capture, while the parent geometry and tags remain unchanged.

## Exact positions

All five retained items use `x = visibleWidth * 0.5` and `y = visibleHeight - 55 - 90 * tag`. On the recovered 1136x640 design surface the centers are `(568,495)`, `(568,405)`, `(568,315)`, `(568,225)`, and `(568,135)`.

`single_select_hero_rune_layout` maps each imported TexturePacker frame by its untrimmed source rectangle and trim offset through the shared centered aspect-fit transform. Runtime opacity is `1` only for visible tags `1` and `2` in this scene state.

## Touch rectangles

The visible atlas rectangle and the Cocos menu-item touch rectangle are not always identical. `CCMenuItemSprite` uses the normal sprite's untrimmed content size, while TexturePacker trimming only changes the visible draw quad. The recovered normal source sizes are:

| career | source size | design-space touch rectangle |
| --- | --- | --- |
| 1 | `79 x 99` | `(528.5,95.5)-(607.5,194.5)` |
| 2 | `87 x 89` | `(524.5,190.5)-(611.5,279.5)` |
| 3 | `75 x 95` | retained geometry only |
| 4 | `111 x 110` | retained geometry only |
| 5 | `137 x 131` | retained geometry only |

Career 2 demonstrates why this matters: its visible normal frame has a 3-pixel top trim, so drawing begins at design Y `193.5`, while the original menu-item content rectangle begins at `190.5`.

The layout helper exposes source-size-based `hit_rect_for_surface()` and `hit_test()` using the same centered aspect-fit transform as rendering. Only runtime-visible tags produce active touch rectangles.

## Selected and pressed frame behavior

Selected and pressed presentation are distinct:

- the selected visible career persistently resolves to `xrfuwenfaguangNN.png`;
- DOWN inside the other visible rune temporarily publishes that rune as pressed, so it also resolves to its glow frame;
- MOVE outside clears only the pressed presentation and MOVE back inside restores it;
- UP, POINTER_UP, CANCEL, locked input and reset clear the pressed presentation;
- after a successful career change, the persistent selected glow moves to the new selected tag;
- selecting a semantic career without a runtime-visible rune clears the persistent visible-rune glow rather than inventing a hidden presentation.

Both selected and pressed presentation tags are atomic because Android touch dispatch/state changes and GLES drawing can run on different threads.

## Runtime boundary

`SingleSelectHeroBaseComposer` extracts all ten normal/glow frames from the imported `Singleselechero.plist/png` atlas; no original image bytes are committed. `single_select_hero_state` publishes the current selected visible career into the presentation layer, while the input bridge independently owns transient pressed feedback.

The native Confirm control is reconstructed separately and routes into `single_select_hero_state::confirm_online()`. CharacterName remains the next presentation block after confirmation.
