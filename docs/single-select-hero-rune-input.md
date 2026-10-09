# SingleSelectHero career-rune input bridge

This increment connects the reconstructed five-rune hit geometry to the reconstructed near/far transition timeline.

While the fresh-account `OpeningDialogue` route owns an active `single_select_hero_state`, it receives touch events before the mutually-exclusive ChooseHero, ManagementLayer and server-selection consumers. The locked OpenTheDoor/Carousel interval still consumes input but cannot change the selected career.

A rune is armed by DOWN/POINTER_DOWN inside its recovered untrimmed `CCMenuItemSprite` rectangle. MOVE clears/restores the pressed tag when the pointer leaves/re-enters that same item. UP/POINTER_UP commits only when it ends inside the armed item; CANCEL clears the gesture.

On commit, the input bridge snapshots the old career, calls `single_select_hero_state::select_career(tag)`, and, only when the selection actually changed, arms:

```text
single_select_hero_transition_timeline::begin_career_change(
    game_clock::tick_count(), generation, oldCareer, newCareer)
```

The timeline therefore remains the sole owner of the recovered near/far deadline choice (179 or 231 fixed-clock ticks). Same-career presses remain handled no-ops and do not restart the transition.

The existing Java ABI is unchanged: the shared `nativeOnServerSelectionTouch` router gives SingleSelectHero first refusal. The GLES-independent core has host smoke coverage for lock ownership, move/cancel behavior, same-career behavior and both near/far timeline arguments.

`pressed_tag()` is intentionally exposed for a later presentation-only compositor change that can switch the already staged `xrfuwenfaguang%02d.png` frames without altering input semantics.
