# SingleSelectHero transition timing

This note records the recovered interaction-unlock timing behind `SingleSelectHero::OpenTheDoor`, `Carousel`, `FunOpenTheDoor`, and `FuncCloseTheDoor`. The repository stores only the clean-room semantic timing relationships; original disassembly is not committed.

## Interaction byte

`menuOpenGC(sender)` first checks the SingleSelectHero interaction byte. A changed career calls:

```text
OpenTheDoor(false, career)
```

and then stores the sender tag unchanged as the selected/current career. `OpenTheDoor(false, ...)` schedules the closing sequence and writes the interaction byte false immediately.

The closing sequence eventually calls `FuncCloseTheDoor(career)`, which forwards the same career into `Carousel(career)`. `Carousel` ends with a `FunOpenTheDoor` callback. That callback invokes:

```text
OpenTheDoor(true, career)
```

`OpenTheDoor(true, ...)` writes the interaction byte true synchronously when the callback fires. The gate therefore reopens at the beginning of the opening sequence scheduled by that call; it does not wait for all opening visuals to finish.

## Initial transition

`initUI()` writes the selected career into the current-career field, clears the interaction byte, and calls `Carousel(selectedCareer)`.

For this normal path the current and target careers match, so the recovered Carousel sequence is:

```text
1.5 s move/ease
0.3 s move
0.6 s move
FunOpenTheDoor callback
```

The callback point is exactly `2.4 s` after the sequence starts. The recompilation fixed clock runs at 35 Hz, so the exact deadline is:

```text
2.4 * 35 = 84 ticks
```

`single_select_hero_transition_timeline::begin_initial()` therefore arms `tick + 84`.

## Changed-career transition

The recovered close path before `FuncCloseTheDoor` is:

```text
2.0 s move/ease
instant FuncBegin callback
0.3 s move
0.39 s move
FuncCloseTheDoor callback
```

That is `2.69 s`, followed by the same `2.4 s` Carousel. The interaction byte is therefore restored at:

```text
2.69 + 2.4 = 5.09 s
```

At 35 Hz this is `178.15` ticks. The first fixed tick that is not earlier than the recovered callback point is `+179`, so `begin_career_change()` arms that deadline.

## Runtime integration

The OpeningDialogue scene bridge now starts the initial timeline in the same frame that it calls `single_select_hero_state::begin(0)`. Every active frame advances the timeline using `game_clock::tick_count()`. When the deadline fires, the bridge calls `single_select_hero_state::complete_transition()`, reproducing the recovered `FunOpenTheDoor -> OpenTheDoor(true, career)` interaction effect.

Selector generations are attached to every timeline so a stale callback from an old scene cannot unlock a newer selector.

The temporary Android fresh-role compatibility overlay intentionally retains its explicit transition collapse. It is a development shim and remains separate from native GLES timing; if the shim clears `transition_pending` early, the scene bridge discards the armed native timeline rather than firing a stale callback later.

## Next boundary

`begin_career_change()` is implemented but not yet called by the GLES rune renderer. The next native-input increment should:

1. derive each rune hit rectangle from its imported normal-frame source size and recovered center;
2. use the staged `xrfuwenfaguangNN` frame only while a valid rune press is active;
3. dispatch `single_select_hero_state::select_career(tag)` on a completed press;
4. when the selection actually changes, arm `begin_career_change(game_clock::tick_count(), selector_generation)`;
5. route the recovered confirm control separately through `confirm_online()`.
