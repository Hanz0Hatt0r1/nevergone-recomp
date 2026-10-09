# SingleSelectHero native career-rune input

This note records the clean-room native input layer for the recovered `SingleSelectHero::menuOpenGC` career controls. Original image bytes and disassembly are not stored in the repository.

## Existing Java router

`GameSurfaceView` already forwards Android `MotionEvent` data through `nativeOnServerSelectionTouch(...)` before the generic TapToStart route. That native function is a first-refusal router for mutually-exclusive reconstructed screens.

SingleSelectHero rune input is added there rather than introducing another Java touch path. Outside the fresh-role `OpeningDialogue` route it immediately returns false and leaves all existing server/role/ChooseHero consumers unchanged.

## Runtime hit rectangles

The shipped menu-item content size is the untrimmed normal-frame size, not the TexturePacker visible trim rectangle. The renderer already has both pieces of information required for exact touch mapping:

- actual `GLSurfaceView` width/height;
- imported normal-frame `source_width/source_height`.

`single_select_hero_rune_layout::quad_for_surface()` now publishes those scalar values atomically while it draws each normal rune. The UI-thread input consumer reads them without accessing GL objects or proprietary pixels.

Touch coordinates are converted through the same centered aspect-fit transform as rendering. A hit uses the exact recovered center for the rune plus half of its untrimmed source width/height.

## Pressed presentation

The original `CCMenuItemSprite` uses:

```text
xrfuwenNN.png           normal
xrfuwenfaguangNN.png    selected/pressed
```

A captured DOWN publishes the pressed tag. The existing compositor already calls `frame_index(tag, false)` on every draw; the shared layout resolves that call to the staged glow frame while the pointer remains inside the same rune rectangle. Moving outside restores normal art, moving back in restores glow, and UP/CANCEL clears it.

The normal-frame source rectangle remains frozen as the hitbox while glow is active, so a differently trimmed pressed frame cannot change pointer ownership mid-gesture.

## Career dispatch

A completed DOWN/UP on the same rune calls:

```text
single_select_hero_state::select_career(tag)
```

The state still owns the recovered OpenTheDoor gate. Therefore a press may visually select the `CCMenuItemSprite` while the gate is locked, but the callback does not mutate career until `input_enabled` is true.

When selection actually changes, the input layer detects the incremented `selection_count` and arms:

```text
single_select_hero_transition_timeline::begin_career_change(
    game_clock::tick_count(),
    selector_generation)
```

which preserves the recovered +179 fixed-tick lock before `FunOpenTheDoor -> OpenTheDoor(true, career)` re-enables `menuOpenGC`.

Re-selecting the already displayed career is a handled no-op and does not arm another transition.

## Pointer ownership

One pointer is captured at a time. DOWN/POINTER_DOWN outside all runes yields to the next native consumer. Once a rune owns a pointer, MOVE/UP/CANCEL for that pointer remain consumed until capture ends. This prevents a rune gesture from falling through into TapToStart or another screen router partway through the gesture.

## Current boundary

This increment makes the five career runes natively interactive and removes the career-selection dependency on the temporary Android compatibility controls. The compatibility overlay remains available as a development fallback while the remaining original presentation is reconstructed.

The next visual/input boundary is the recovered SingleSelectHero confirm control. Its exact resource, geometry and pressed-state contract should be recovered before wiring `single_select_hero_state::confirm_online()` into the GLES screen; no guessed confirm button is introduced here.
