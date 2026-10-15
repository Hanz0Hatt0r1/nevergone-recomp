# Tutorial C tap -> live player jump action

## Scope

This is the bounded runtime transition from recovered Controls input into the
imported tutorial jump action and then back into the original LessonLayer
progression gate. The important distinction is that **pressing C requests the
jump, while entering live MEPLAYERSTATE `0x20` completes the Jump lesson**.

A/B combat outcomes and the later tutorial enemy roster remain separate
reconstruction work.

## Evidence chain

The clean-room chain is based on the shipped ARMv7 behavior:

1. the recovered Controls C button is original function state `9`;
2. `MEPlayer::setFunctionState(9)` dispatches `setActionState(0x20, false)`
   after its common gates;
3. direct Thumb recovery of `MEPlayer::setActionState` shows current state
   `0x1b` accepts requested non-forced state `0x20` and stores it to MEPlayer
   `+0x278`;
4. `MEPlayer::setPlayerStateWithIdx` uses MEPLAYERSTATE directly as the
   zero-based `MeActionsSystem` array index;
5. `MEPlayer::createActions` fills that array from `action01`, `action02`, ...
   in order;
6. therefore state `0x20` resolves `action33` in the selected career's
   `pNN_actions.plist`;
7. the recovered `LessonLayer::nextLesson(MEPLAYERSTATE)` Jump branch advances
   only when its state argument is exactly `0x20`.

The initial player state is `0x1b`, which resolves `action28`.

## Runtime request boundary

`game_scene_action_input` receives the semantic C tap only after the reconstructed
Controls hit-test has established which original button was touched. While the
coarse tutorial session is Interactive and the current gameplay step is Jump,
the tap increments the monotonic:

```text
tutorial_jump_request_count
```

It **does not** call `tutorial_gameplay_state::observe(kJumpPerformed)` at this
point. The tutorial therefore remains on Jump while the imported jump action is
being resolved and loaded.

Keeping a monotonic request count still preserves fast input correctly: a
DOWN/UP pair may occur completely between two render/staging passes without
being lost.

## Live MEPlayer state boundary

`meplayer_live_action_state` is synchronized to the concrete reconstructed
player-spawn generation. A fresh spawn begins with desired/current state
`0x1b`; the current jump-request count is captured at synchronization so stale
input from an older player generation cannot replay.

A new unseen jump request is accepted only while both current and desired live
states are `0x1b`. It queues desired state `0x20`. Additional requests arriving
while that transition is pending are retained diagnostically but cannot queue
parallel jump actions.

`PlayerActionRuntimeLoader` asks native code for the desired state, resolves
state `0x20` through the imported player action set, parses `action33`, and then
calls the native commit boundary. A missing/corrupt action document therefore
cannot advance the lesson: the current state remains `0x1b` and Jump stays
active.

On a successful commit, `meplayer_live_action_state.current_action_state`
becomes `0x20`. Only after that commit does the bridge call:

```text
game_scene_action_input::observe_live_meplayer_state(0x20)
```

That function checks that the tutorial is still Interactive/Jump and publishes
`kJumpPerformed`. The tutorial then advances exactly once to Attack. Repeated
commits/observations of `0x20` are idempotent because Attack now owns the
state-machine step.

This matches the original distinction between Controls input and the subsequent
`LessonLayer::nextLesson(MEPLAYERSTATE == 0x20)` gate instead of treating a raw
button tap as proof that the player action actually changed.

## Rendering and completion

The committed jump action participates in the existing complete live action
playback path. `me_actions_live_playback` advances imported `action33` frames on
the fixed 35 Hz game clock and the player render queue resolves the current
frame/atlas transform. When the imported jump action reports its recovered done
state, `meplayer_live_action_state::observe_action_completion()` queues the
normal return to initial state `0x1b`.

## Validation

Two host smokes cover the split boundary:

- `game_scene_action_input_smoke.cpp` proves that a C tap creates one request
  but leaves the tutorial on Jump until live state `0x20` is observed;
- `tutorial_jump_live_state_integration_smoke.cpp` exercises the complete
  request -> desired state -> action commit -> Jump-to-Attack transition and
  verifies repeated `0x20` observations cannot advance a second step.

`meplayer_live_action_state_smoke.cpp` separately covers request de-duplication,
spawn-generation seeding and the recovered jump-action completion back to
`0x1b`.

## Next tutorial boundary

After the live `0x20` gate, the original Jump callback creates the recovered
three-enemy Attack wave. `tutorial_enemy_spawn_plan` already contains its exact
positions/delays, and `enemy_health_state` contains the required `isDead()`
semantics, but production code does not yet instantiate that plan into a live
enemy roster. Consequently Attack remains correctly objective-gated rather
than being advanced from raw A-button input.
