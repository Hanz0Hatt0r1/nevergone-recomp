# SingleSelectHero career-rune input bridge

This increment connects the reconstructed five-rune hit geometry to the reconstructed near/far transition timeline and pressed-rune presentation.

While the fresh-account `OpeningDialogue` route owns an active `single_select_hero_state`, it receives touch events before the mutually-exclusive ChooseHero, ManagementLayer and server-selection consumers. The locked OpenTheDoor/Carousel interval still consumes input but cannot change the selected career.

A rune is armed by DOWN/POINTER_DOWN inside its recovered untrimmed `CCMenuItemSprite` rectangle. MOVE clears/restores the pressed tag when the pointer leaves/re-enters that same item. UP/POINTER_UP commits only when it ends inside the armed item; CANCEL clears the gesture.

On commit, the input bridge snapshots the old career, calls `single_select_hero_state::select_career(tag)`, and, only when the selection actually changed, arms:

```text
single_select_hero_transition_timeline::begin_career_change(
    game_clock::tick_count(), generation, oldCareer, newCareer)
```

The timeline therefore remains the sole owner of the recovered near/far deadline choice (179 or 231 fixed-clock ticks). Same-career presses remain handled no-ops and do not restart the transition.

The existing Java ABI is unchanged: the shared `nativeOnServerSelectionTouch` router gives SingleSelectHero first refusal. The GLES-independent core has host smoke coverage for lock ownership, move/cancel behavior, same-career behavior and both near/far timeline arguments.

## Pressed/glow presentation

The shipped atlas already supplies paired frames for each career: `xrfuwen%02d.png` and `xrfuwenfaguang%02d.png`. The compositor already selected rune frames through `single_select_hero_rune_layout::frame_index(tag, false)`, so pressed presentation is connected at that shared frame-selection boundary rather than duplicating gesture state inside GLES code.

The input router publishes its pressed tag through a C++17 inline atomic. While a rune is held inside its recovered hit rectangle, `frame_index(tag, false)` resolves that one career to the staged glow frame; all other careers remain on their normal frame. MOVE-out, UP/POINTER_UP, CANCEL, route reset and the locked transition path clear the atomic immediately.

This keeps the UI-thread gesture mutex out of the GL thread while making the visible pressed state follow the same source of truth tested by the input smoke. An explicit `frame_index(tag, true)` still forces the pressed frame for deterministic layout tests.
