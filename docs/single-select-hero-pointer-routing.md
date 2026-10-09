# SingleSelectHero rune pointer routing

This increment connects the already recovered SingleSelectHero rune hit rectangles and transition deadlines to the existing Android MotionEvent path. It does not guess new geometry or shorten the recovered transition gate.

## Inputs reused from prior evidence

PR #167 established the five `CCMenuItemSprite` normal-source touch sizes and centers:

| career | source size | design-space hit rectangle |
| --- | --- | --- |
| 1 | `79 x 99` | `(528.5,95.5)-(607.5,194.5)` |
| 2 | `87 x 89` | `(524.5,190.5)-(611.5,279.5)` |
| 3 | `75 x 95` | `(530.5,277.5)-(605.5,372.5)` |
| 4 | `111 x 110` | `(512.5,360)-(623.5,470)` |
| 5 | `137 x 131` | `(499.5,439.5)-(636.5,570.5)` |

PR #169 established the native input deadlines on the 35 Hz fixed clock:

- initial Carousel unlock: `+84` ticks;
- changed-career close + Carousel unlock: `+179` ticks.

The pointer router consumes both contracts directly.

## MotionEvent ownership

`nativeOnServerSelectionTouch(...)` is already the Java first-refusal path before generic TapToStart input. It now asks `single_select_hero_pointer_router` first. SingleSelectHero, ChooseHero, ManagementLayer role selection, and server selection are mutually exclusive semantic routes, so this does not introduce a second Android event stream.

The router implements the Cocos menu gesture shape without inventing a larger touch area:

- DOWN / POINTER_DOWN is accepted only while `single_select_hero_state.input_enabled` is true and the pointer is inside one exact rune hit rectangle;
- one pointer captures one rune; another pointer cannot steal the gesture;
- MOVE keeps the original rune armed and tracks whether the pointer remains inside it;
- UP / POINTER_UP activates only when released inside the same rune;
- release outside cancels activation;
- CANCEL / OUTSIDE clears capture;
- while CharacterNameLayer is active, the selector beneath it cannot receive rune input.

Once a DOWN has been captured, uncommon MotionEvent actions remain owned instead of leaking into another route.

## Career change and timing

On a successful release the exact sender tag `1..5` is passed unchanged to `single_select_hero_state::select_career(tag)`.

If the selected career actually changed, the recovered state immediately sets `input_enabled=false` and `transition_pending=true`. The router then calls:

```text
single_select_hero_transition_timeline::begin_career_change(
    game_clock::tick_count(),
    selector_generation)
```

The existing scene bridge advances that timeline and unlocks the selector at the recovered `+179` tick callback. The native rune path therefore does **not** use `fresh_role_compat_state`'s temporary immediate transition collapse.

Selecting the already-current career is owned but starts no new transition.

## Surface coordinates

The hit helper maps the 1136x640 design rectangles through the shared centered aspect-fit transform. MotionEvent X/Y are local to `GameSurfaceView`, so native code needs the actual view dimensions in the same coordinate system.

As a transitional plumbing step, the already temporary `FreshRoleCompatOverlay` publishes the attached `GameSurfaceView` width/height to `single_select_hero_pointer_router`. This does not alter selection semantics. Once career Confirm and CharacterName controls have native scene routing and the compatibility overlay is removed, the size publisher should move to the common surface lifecycle bridge.

## Tests

`single_select_hero_touch_state_smoke` verifies pointer capture, second-pointer rejection, exit/re-entry, release-inside activation, release-outside cancellation, and cancel handling.

`single_select_hero_pointer_router_smoke` verifies:

- exact rune hit routing on a 1136x640 surface;
- career change to tag `3`;
- immediate input lock after change;
- arming of the recovered `kCareerChange` timeline at current fixed tick with `+179` deadline;
- release-outside cancellation;
- same-career selection without another transition;
- refusal during the initial locked Carousel;
- refusal while CharacterNameLayer is modal.

Fast CI runs both tests; main-branch full CI remains the Android JNI/Gradle/NDK and 16 KiB gate.

## Remaining interaction boundary

The next evidence-backed SingleSelectHero increments are:

1. render `xrfuwenfaguangNN.png` while the captured pointer is inside its armed rune;
2. recover/connect the Confirm button hit rectangle and `menuConfirm` touch path;
3. replace the temporary compatibility career controls and surface-size publisher once native Confirm is available;
4. continue wiring the already recovered CharacterName layout into a native compositor/edit-box path.
