# Tutorial gameplay progression

## Evidence boundary

This document records the clean-room state contract recovered from frozen original-game runtime captures plus strings/assets already resident in the original process. No proprietary screenshots, heap dumps, assets, Lua blobs, or disassembly are committed.

The observed startup chain is:

`SingleSelectHero / CharacterName -> Loading -> cp_lesson / LessonLayer -> post-lesson DialogueUI -> interactive tutorial gameplay -> normal gameplay`

The post-lesson DialogueUI is modal. A captured first line is progressively typeset while the complete sentence is already present in runtime memory, so dialogue completion is modeled as an explicit event rather than a guessed timer.

## Session lifecycle

`tutorial_session_state` preserves the coarse original boundaries independently of the individual gameplay prompts:

`Loading -> LessonOverlay -> Dialogue -> Interactive -> Complete`

Loading, LessonOverlay, and Dialogue are modal phases and report gameplay input as blocked. Interactive unlocks gameplay input and delegates progression to `tutorial_gameplay_state`. The session may enter Complete only after the interactive state machine itself has reached its terminal step. A new loading session increments a generation counter and clears stale gameplay tutorial state.

The startup handoff is deliberately kept separate from this lifecycle contract in this change: the original loading completion trigger and LessonLayer bootstrap still need an evidence-backed runtime caller instead of a fabricated timer.

## Observed interactive order

Frozen captures directly establish the first five interactive steps and their presentation:

1. **Run** — `Double-tap a direction to run.`; joystick highlighted.
2. **Jump** — `Tap C to jump.`; C highlighted.
3. **Attack** — `Tap A to attack. Eliminate all the enemies in front of you.`; A highlighted and tutorial enemies present.
4. **Block** — `Tap and hold B to [Block] the enemy attack.`; B highlighted and a live enemy remains.
5. **Counter-attack** — `[Counter-attack] by attacking (tap A) immediately after a successful block.`; A highlighted following the block lesson.

The same frozen runtime state also contains the subsequent English tutorial strings and corresponding localized table entries/resources:

6. **Dodge** — `Tap left or right while holding B to [Dodge] enemy attacks.`
7. **Air Attack** — `Tap C to jump, then tap A to attack in the air. Use [Air Attack] to eliminate the bat.`
8. **Execution preparation** — `Tap the potion in the item slot to max your rage so that you can unleash your [Execution].`
9. **Execution** — `Swipe from "A" to "C" to launch [Execution] when your [Rage] is full.`

This order is represented as a bounded project state contract. If later original evidence contradicts a transition after Counter-attack, update the table/state machine rather than inventing timing behavior.

## Completion events

The reconstruction intentionally advances on gameplay outcomes, not a fixed delay:

| Step | Required completion event |
| --- | --- |
| Dialogue | post-LessonLayer dialogue sequence completed |
| Run | run action successfully performed |
| Jump | jump action successfully performed |
| Attack | tutorial enemy group defeated |
| Block | enemy hit successfully blocked |
| Counter-attack | successful counter-attack in the post-block window |
| Dodge | successful dodge while using the block/dodge input |
| Air Attack | tutorial bat defeated by an air attack |
| Execution preparation | tutorial item raises Rage to the required full state |
| Execution | execution action successfully launched |

A raw button press is therefore insufficient for several steps. In particular, Attack waits for the enemy objective, Block waits for a successful blocked attack, Counter-attack waits for the actual counter result, and Execution waits for the gameplay action rather than only the A-to-C gesture beginning.

## Live integration

The **Run** step is now connected to the reconstructed native joystick combo path. `game_scene_player_joystick_input` already recognizes the shipped same-direction double-tap window and increments `fast_reactivation_count` only when the recovered fast movement state is entered. A new fast reactivation publishes `kRunPerformed` when, and only when, the tutorial currently owns `Step::kRun`; the tutorial then advances to **Jump**.

This keeps the tutorial dependent on the same movement state that drives player motion rather than a separate UI-only double-tap detector. Publication occurs outside the joystick mutex so tutorial state cannot participate in the input lock order. Fast reactivations outside the Run step are ignored by the tutorial contract.

## Runtime contract

`tutorial_gameplay_state` exposes:

- the current tutorial step;
- the evidence-backed prompt and highlighted target control;
- explicit gameplay completion events;
- transition/rejected-event counters for diagnostics;
- a generation counter so scene restarts cannot reuse stale tutorial events.

`tutorial_session_state` additionally exposes the coarse tutorial phase, modal-input gate, lifecycle transition counters, and lifecycle generation.

The modules deliberately do **not** yet decide rendering geometry or wire themselves directly into every control/combat callback. Remaining Jump/combat/item/Execution connections should feed the semantic events above when the corresponding reconstructed gameplay systems can prove the event occurred.
