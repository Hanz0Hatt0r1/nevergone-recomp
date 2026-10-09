# SingleSelectHero native career-rune input

This note records the native pointer layer for the recovered `SingleSelectHero::menuOpenGC` career controls. It builds on the exact source-size hit rectangles recovered in PR #167 and the near/far `OpenTheDoor` / `Carousel` timing represented by PR #169 and corrected by PR #170.

## Existing native router

`GameSurfaceView` already forwards Android `MotionEvent` data through `nativeOnServerSelectionTouch(...)` before the generic touch path. The SingleSelectHero career runes join that existing first-refusal router instead of adding another Java input bridge. Outside the fresh-role `OpeningDialogue` route the consumer immediately returns false.

## Exact hit geometry

PR #167 established that the `CCMenuItemSprite` hit rectangle uses the untrimmed normal sprite size. The recovered sizes for careers 1..5 are `79x99`, `87x89`, `75x95`, `111x110`, and `137x131`.

The existing rune compositor calls `single_select_hero_rune_layout::quad_for_surface()` with the actual `GLSurfaceView` dimensions. That helper now publishes the current surface width/height as atomic scalar state. The UI-thread pointer consumer reuses those live dimensions together with the PR #167 source sizes and `hit_test()`. It does not access GL state or proprietary image bytes from the Android UI thread.

## Pressed-frame behavior

The shipped `CCMenuItemSprite` pairs:

```text
xrfuwenNN.png          normal
xrfuwenfaguangNN.png   selected/pressed
```

A captured DOWN publishes the pressed rune tag. The compositor continues using its existing `frame_index(tag, false)` call; the layout helper resolves that to the staged glow frame while the captured pointer remains inside the rune.

Moving outside restores normal art, moving back inside restores glow, and UP/CANCEL clears the transient state. The glow is never treated as a persistent selected-career marker.

## Career dispatch

A completed DOWN/UP on the same rune calls:

```text
single_select_hero_state::select_career(tag)
```

The selector state remains authoritative for the recovered interaction gate. While `OpenTheDoor` / `Carousel` owns that gate, rune press presentation can still occur but the semantic career mutation is rejected.

When a release actually changes career, the input layer forwards both careers to the recovered transition model:

```text
begin_career_change(
    game_clock::tick_count(),
    selector_generation,
    old_career,
    new_career)
```

PR #170 then chooses the correct deadline from career distance. Near changes use `+179` fixed ticks, while far changes use `+231`. The input layer does not duplicate that policy. Re-selecting the same career is a handled no-op and does not arm a transition.

## Pointer ownership

Only one pointer may own the rune menu at a time. A DOWN outside all five recovered rectangles yields to the next native consumer. Once captured, MOVE/UP/CANCEL for that pointer remain consumed until the gesture finishes so an in-progress rune gesture cannot fall through into TapToStart or another mutually-exclusive selector.

Scene reset/begin clears both pointer ownership and the atomic pressed/surface state.

## Current boundary

The five career runes are now natively selectable without depending on the temporary Android compatibility controls. The compatibility overlay remains useful as a development fallback while the remaining original presentation is reconstructed.

The next missing original SingleSelectHero control is `menuConfirm`. Its exact resource, geometry and selected-state contract should be recovered before wiring `single_select_hero_state::confirm_online()` into the GLES layer; this increment does not add a guessed confirm button.
