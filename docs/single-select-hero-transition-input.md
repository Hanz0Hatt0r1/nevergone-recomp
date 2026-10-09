# SingleSelectHero transition gate and career input

This note records the clean-room reconstruction of the shipped `SingleSelectHero::OpenTheDoor`, `menuOpenGC`, `FuncCloseTheDoor`, `Carousel` and `initUI` interaction boundary. It stores semantic behavior and timing only; no original binary or proprietary texture bytes are added to the repository.

## Recovered gate behavior

`initUI()` copies the initial selected career into the Carousel field, clears the `menuOpenGC` interaction byte, and immediately calls `Carousel(selectedCareer)`. The initial old/new career distance is therefore zero.

`menuOpenGC(sender)` first checks the interaction byte. When the gate is open and a different career is clicked, it:

1. preserves the previously selected career;
2. calls `OpenTheDoor(false, previousCareer)`;
3. writes the sender tag unchanged as the new selected career.

`OpenTheDoor(false, ...)` clears the interaction gate immediately. Its close-door action lasts `2.0` seconds and then calls `FuncCloseTheDoor`, which forwards the preserved previous career to `Carousel(previousCareer)`.

The final Carousel callback calls `FunOpenTheDoor`, which forwards to `OpenTheDoor(true, ...)`. The interaction byte is restored immediately when that function is entered; the menu does not wait for any later open-door visual animation before becoming selectable again.

## Recovered Carousel timing

`Carousel(oldCareer)` reads the newly selected career and computes the absolute career-tag distance:

```text
distance = abs(newCareer - oldCareer)
```

The first Carousel leg is:

- `1.5 s` when `distance <= 2`;
- `3.0 s` when `distance > 2`.

It is followed by confirmed `0.3 s` and `0.6 s` legs before the callback that restores the input gate.

Therefore the reconstructed gate durations are:

- initial `initUI` Carousel: `1.5 + 0.3 + 0.6 = 2.4 s`;
- changed career with distance `<= 2`: `2.0 + 1.5 + 0.3 + 0.6 = 4.4 s`;
- changed career with distance `> 2`: `2.0 + 3.0 + 0.3 + 0.6 = 5.9 s`.

The runtime evaluates those totals against the existing fixed 35 Hz `game_clock` and calls `single_select_hero_state::complete_transition()` at the first fixed tick at or after the recovered duration.

## Five career rune hit bounds

The shipped `Singleselechero.plist` provides the untrimmed source size used by each `CCMenuItemSprite`. Those source sizes, rather than the trimmed visible atlas rectangles, define the reconstructed touch bounds:

| Career tag | Source size |
| ---: | ---: |
| 1 | 79 x 99 |
| 2 | 87 x 89 |
| 3 | 75 x 95 |
| 4 | 111 x 110 |
| 5 | 137 x 131 |

The menu-item centers remain the recovered 1136x640 design coordinates `(568,495)`, `(568,405)`, `(568,315)`, `(568,225)` and `(568,135)`. Surface hit boxes use the same aspect-fit mapping as the rune renderer.

## Touch ownership

The fresh-account `OpeningDialogue` route owns the full touch stream while `SingleSelectHero` is active. During the locked Carousel/door interval, touches are consumed but cannot change careers. This prevents a locked rune touch from falling through into mutually exclusive `TapToStart`, server-selection or ChooseHero consumers.

A gesture is armed on DOWN/POINTER_DOWN inside one rune. MOVE updates the pressed tag only while the pointer remains inside the same recovered item bound. UP/POINTER_UP selects the career only when it ends inside that armed bound; CANCEL clears the gesture. Selecting the already displayed career remains the recovered handled no-op and does not start a new transition.

The input core is GLES-independent and has host smoke coverage. Its Android adapter obtains the current render-surface dimensions from the existing thread-safe render diagnostics so no GL-owned texture or compositor state is read from the UI thread.

## Current boundary

This increment restores the interaction gate and actual career selection for all five recovered runes. The staged `xrfuwenfaguang%02d.png` pressed frames are not yet switched by the compositor; `single_select_hero_input::pressed_tag()` exposes the exact state needed for that presentation-only follow-up without changing selection semantics.
