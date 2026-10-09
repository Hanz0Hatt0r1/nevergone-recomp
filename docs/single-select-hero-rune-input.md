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

## Pressed presentation

The input router now publishes its current pressed tag into the shared rune presentation state. The existing compositor already asks `single_select_hero_rune_layout::frame_index(tag, false)` for every draw; that helper now selects the staged `xrfuwenfaguang%02d.png` frame only when the same tag is currently pressed.

This remains presentation-only:

- DOWN inside an enabled rune switches only that rune to the glow frame;
- MOVE outside returns it to the normal frame;
- MOVE back inside restores the glow frame;
- UP/POINTER_UP clears the glow before the career-change transition proceeds;
- CANCEL, route loss, locked input and explicit reset all clear the glow;
- the selected career is not represented by this glow after the gesture ends, matching the recovered `CCMenuItemSprite` normal/selected behavior.

The presentation tag is atomic because touch routing and GLES drawing can run on different threads. It carries only the current visual tag and does not duplicate or replace `single_select_hero_state`.

Host coverage now checks the presentation tag through the real input-router sequence as well as normal/glow frame-index selection.
