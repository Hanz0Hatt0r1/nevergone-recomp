# SingleSelectHero transition timing

This note records the recovered interaction-unlock timing behind `SingleSelectHero::OpenTheDoor`, `Carousel`, `FunOpenTheDoor`, and `FuncCloseTheDoor`. The repository stores only clean-room semantic timing relationships; original disassembly is not committed.

## Interaction byte

`menuOpenGC(sender)` first checks the SingleSelectHero interaction byte. On a changed career, the old selected career remains in the argument register and the shipped handler calls:

```text
OpenTheDoor(false, oldCareer)
```

before storing the sender tag as the new selected/current career. `OpenTheDoor(false, ...)` schedules the close sequence and writes the interaction byte false immediately.

The close sequence eventually calls `FuncCloseTheDoor(oldCareer)`, which forwards that old career into `Carousel(oldCareer)`. At that point the current-career field already contains the new career, so Carousel can choose its movement duration from `abs(newCareer - oldCareer)`. Carousel ends with `FunOpenTheDoor`, which calls `OpenTheDoor(true, ...)`; that call writes the interaction byte true synchronously.

## Initial transition

`initUI()` stores the selected career, clears the interaction byte, and calls `Carousel(selectedCareer)`. Old and new careers are equal, so the recovered sequence is:

```text
1.5 s primary move/ease
0.3 s move
0.6 s move
FunOpenTheDoor callback
```

The callback point is exactly `2.4 s`, or `84` ticks on the reconstructed 35 Hz fixed clock. `begin_initial(tick, generation, selectedCareer)` therefore arms `tick + 84`.

## Changed-career transition

The close path before `FuncCloseTheDoor` is:

```text
2.0 s move/ease
instant FuncBegin callback
0.3 s move
0.39 s move
FuncCloseTheDoor callback
```

This phase takes `2.69 s`. The following Carousel has two recovered primary durations:

- `abs(newCareer - oldCareer) <= 2`: `1.5 + 0.3 + 0.6 = 2.4 s`;
- `abs(newCareer - oldCareer) > 2`: `3.0 + 0.3 + 0.6 = 3.9 s`.

Therefore the interaction unlock occurs at:

```text
near change: 2.69 + 2.4 = 5.09 s -> ceil(5.09 * 35) = 179 ticks
far change:  2.69 + 3.9 = 6.59 s -> ceil(6.59 * 35) = 231 ticks
```

`begin_career_change(tick, generation, oldCareer, newCareer)` records both careers and selects the matching fixed-clock deadline. Same-career or invalid-career requests do not arm a transition.

## Runtime integration

The OpeningDialogue scene bridge starts the initial timeline in the same frame that it calls `single_select_hero_state::begin(0)`, passing the resulting selected career into `begin_initial`. Every active frame advances the timeline using `game_clock::tick_count()`. When the deadline fires, the bridge calls `single_select_hero_state::complete_transition()`, reproducing the recovered `FunOpenTheDoor -> OpenTheDoor(true, ...)` gate effect.

Selector generations are attached to every timeline so a stale callback from an old scene cannot unlock a newer selector. The temporary Android fresh-role compatibility overlay may deliberately collapse the transition early; if it clears `transition_pending`, the native bridge discards the armed deadline rather than firing it later.

## Next boundary

The exact rune hit rectangles are already reconstructed. The next native-input increment should:

1. use the staged `xrfuwenfaguangNN` frame while a valid rune press is active;
2. dispatch `single_select_hero_state::select_career(tag)` on a completed DOWN/UP press;
3. preserve the old career before dispatch;
4. when the career actually changes, arm `begin_career_change(game_clock::tick_count(), generation, oldCareer, newCareer)`;
5. route the recovered confirm control separately through `confirm_online()`.
