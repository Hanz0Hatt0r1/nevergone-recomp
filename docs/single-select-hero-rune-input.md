# SingleSelectHero native career-rune input

This note records the native pointer layer for the recovered `SingleSelectHero::menuOpenGC` career controls. It builds on the exact source-size hit rectangles recovered in PR #167 and the OpenTheDoor/Carousel timeline reconstructed in PR #169.

## Router

`GameSurfaceView` already forwards `MotionEvent` data through `nativeOnServerSelectionTouch(...)` before the generic touch route. SingleSelectHero joins that existing first-refusal native router instead of adding another Java input bridge. Outside the fresh-role `OpeningDialogue` route it returns false immediately.

## Surface mapping

PR #167 recovered the normal `CCMenuItemSprite` source sizes used as touch rectangles: `79x99`, `87x89`, `75x95`, `111x110`, and `137x131` for careers 1..5.

The compositor already calls `single_select_hero_rune_layout::quad_for_surface()` with the actual `GLSurfaceView` size. That helper now publishes the surface width/height atomically. The UI-thread pointer consumer combines those live surface dimensions with the PR #167 source sizes and the same centered aspect-fit transform, avoiding duplicated Java surface plumbing or GL calls from the UI thread.

## Pressed frame

The shipped `CCMenuItemSprite` uses `xrfuwenNN.png` as normal art and `xrfuwenfaguangNN.png` as selected/pressed art. A captured DOWN publishes the active tag. The existing compositor still calls `frame_index(tag, false)`, but the shared layout resolves the staged glow frame while that tag is pressed.

Moving outside the captured rune restores normal art; moving back inside restores glow. UP and CANCEL clear the transient pressed tag. This is not used as a persistent current-career highlight.

## Dispatch and transition gate

A completed DOWN/UP on the same rune calls `single_select_hero_state::select_career(tag)`. The state remains authoritative for the recovered `OpenTheDoor` interaction byte, so a visual press can be captured while the gate is locked but the career mutation is rejected on release.

When a release actually changes career, the input layer observes the incremented `selection_count` and arms:

```text
single_select_hero_transition_timeline::begin_career_change(
    game_clock::tick_count(),
    selector_generation)
```

That keeps the selector locked for the recovered +179 fixed ticks before the `FunOpenTheDoor -> OpenTheDoor(true, career)` callback point. Re-selecting the same career is a handled no-op and does not create another transition.

## Pointer ownership

One pointer is captured at a time. A DOWN outside every rune yields to the next native consumer. After capture, MOVE/UP/CANCEL for that pointer stay consumed until the gesture finishes so a rune gesture cannot fall through into TapToStart or another mutually-exclusive screen halfway through.

## Boundary

The career runes are now natively interactive. The temporary Android fresh-role compatibility overlay remains as a development fallback, but native career selection no longer depends on it.

The next missing original control on this screen is `menuConfirm`. Its resource, geometry, pressed state and callback surface should be recovered before wiring `single_select_hero_state::confirm_online()` into the GLES layer; no guessed confirm button is introduced here.
