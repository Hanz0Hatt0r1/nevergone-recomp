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

## Door motion versus input unlock

The original `OpenTheDoor(true, ...)` call reenables the selector's
interaction byte synchronously when invoked by `FunOpenTheDoor`, but
the newly scheduled **opening visual sequence** continues afterward.

The exact `CCMoveTo/CCDelayTime` durations recovered from the full
original ARM32 `OpenTheDoor` export sum to:

```text
opening lid: 0.2 + 0.5 + 2.0 + 0.3 + 0.39 = 3.39 seconds
```

Therefore the 84/179/231-tick deadlines in this document are
**interaction gate** deadlines, not the final moment at which the
lid is fully open. Do not extend those deadlines by 3.39 seconds:
that would change original input behavior. Initial visual activity
can continue through `2.4 + 3.39 = 5.79` seconds (assuming complete,
uninterrupted actions); a near career change through
`2.69 + 2.4 + 3.39 = 8.48` seconds.

`single_select_hero_coffin_motion.h` records the recovered spatial
targets and easing for the reel and door. The current GLES compositor
uses `single_select_hero_lid_presentation.h` to render the active lid,
with a separate `VisualSnapshot` retained beyond the interaction gate.
Ambiguous starts from overlapping Cocos actions abstain rather than
force an unproven position. Rendering the other covers, masks and callback
effects remains separate work.

## Native GLES presentation bridge (2026-10-19)

The original lid has a visible state lifetime longer than the
original input lock. The scene's current reconstruction therefore
exposes **two distinct snapshots**:

- `Snapshot` still owns interaction locking, ending at the existing
  84/179/231-tick gate. Its schema and callback behavior are unchanged.
- `VisualSnapshot` pins `phase`, selector generation, old/new careers,
  initial game-clock tick, and full expected visual completion:
  initial **203 ticks**, changed-near **297 ticks**, changed-far
  **350 ticks**. It remains available after the input unlock, but resets
  with scene/generation invalidation.

`single_select_hero_lid_presentation.h` samples this visual snapshot
using the original `CCMoveTo` / easing model. The compositor places the
original uploaded `xrguangai.png` active cover in front of the hero
and behind the rune/menu controls, with the original TexturePacker
trim geometry and parent reel translation.

If a second selection begins while the previous cover is still running
actions, the original `numberOfRunningActions()` check makes its start
position conditional. The new visual snapshot marks that transition
`initial_positions_verified=false`, so **no speculative cover is drawn**
for the overlapping action. Character selection and its independent
input-gate deadlines continue unaffected.

This reproduces the proven active-lid movement, not the complete original
five-cover visibility, clipping or all callback-driven fades. Device
visual parity remains unverified until tested on actual imported assets.

The fast CI covers initial input-unlock persistence, the
early-interruption no-draw branch, near-reel close/open boundaries,
scene-generation invalidation, and independent visual reset.

## Runtime integration

The OpeningDialogue scene bridge starts the initial timeline in the same frame that it calls `single_select_hero_state::begin(0)`, passing the resulting selected career into `begin_initial`. Every active frame advances the timeline using `game_clock::tick_count()`. When the deadline fires, the bridge calls `single_select_hero_state::complete_transition()`, reproducing the recovered `FunOpenTheDoor -> OpenTheDoor(true, ...)` gate effect.

Selector generations are attached to every timeline so a stale callback from an old scene cannot unlock a newer selector. The temporary Android fresh-role compatibility overlay may deliberately collapse the transition early; if it clears `transition_pending`, the native bridge discards the armed deadline rather than firing it later.

## Remaining visual boundary

The visible rune hit rectangles, pressed/selected glow, and confirm
input have their own reconstructed paths. The remaining task here is
to connect the recovered door and reel motion to the original parent,
clipping and opacity hierarchy without allowing a lid to obscure
the avatar incorrectly. That should not change the already recovered
interaction unlock deadlines.
