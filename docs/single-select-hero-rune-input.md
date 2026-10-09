# SingleSelectHero career-rune input bridge

This increment connects the already reconstructed SingleSelectHero rune hit geometry to the already reconstructed transition timeline. It does not change either evidence set.

## Input ownership

While the fresh-account `OpeningDialogue` route owns an active `single_select_hero_state`, SingleSelectHero consumes the full touch stream before ChooseHero, ManagementLayer role selection and server selection. This matches the mutually-exclusive scene ownership and prevents a locked career touch from falling through into TapToStart.

When the recovered interaction gate is closed, events are consumed without changing `selected_career`.

## Gesture contract

The five menu items use the untrimmed source sizes recovered from `Singleselechero.plist`:

- career 1: `79 x 99`;
- career 2: `87 x 89`;
- career 3: `75 x 95`;
- career 4: `111 x 110`;
- career 5: `137 x 131`.

Hit testing reuses `single_select_hero_rune_layout::hit_test()` and its 1136x640 aspect-fit transform.

A rune is armed by DOWN/POINTER_DOWN inside its rectangle. MOVE clears/restores the pressed tag as the pointer leaves/re-enters the armed rectangle. UP/POINTER_UP commits only when it ends inside the same rectangle. CANCEL clears the gesture.

## Career change -> recovered timeline

On a committed rune:

1. snapshot the current selector;
2. call `single_select_hero_state::select_career(tag)`;
3. snapshot the resulting selector;
4. if the selected career actually changed and `transition_pending` became true, call `single_select_hero_transition_timeline::begin_career_change(game_clock::tick_count(), generation)`.

This joins the previously separate state and timing reconstructions without duplicating transition constants in the input layer. Clicking the already selected career remains the recovered handled no-op and does not restart the timeline.

## Android adapter

The existing Java input plumbing still calls `nativeOnServerSelectionTouch`. That shared native router now gives SingleSelectHero first refusal. No Java ABI changes are needed.

The input core is GLES-independent and host-testable. The small Android surface adapter reads the current `render surface: WxH` value from the existing thread-safe render diagnostics and then calls the pure surface-aware input function. This avoids reading GL-owned compositor texture state from the UI thread.

`pressed_tag()` is retained as a presentation boundary for a later compositor-only change that can switch the already staged `xrfuwenfaguang%02d.png` frames without changing input semantics.
