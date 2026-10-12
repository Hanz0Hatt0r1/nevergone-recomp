# Tutorial gameplay progression

## Evidence boundary

This document records the clean-room state contract recovered from frozen original-game runtime captures plus strings/assets already resident in the original process. No proprietary screenshots, heap dumps, assets, Lua blobs, or disassembly are committed.

The observed startup chain is:

`SingleSelectHero / CharacterName -> Loading -> cp_lesson / LessonLayer -> post-lesson DialogueUI -> interactive tutorial gameplay -> normal gameplay`

The post-lesson DialogueUI is modal. A captured first line is progressively typeset while the complete sentence is already present in runtime memory, so dialogue completion is modeled as an explicit event rather than a guessed timer.

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

## Runtime contract

`tutorial_gameplay_state` exposes:

- the current tutorial step;
- the evidence-backed prompt and highlighted target control;
- explicit gameplay completion events;
- transition/rejected-event counters for diagnostics;
- a generation counter so scene restarts cannot reuse stale tutorial events.

The module deliberately does **not** yet decide rendering geometry or wire itself directly into every control/combat callback. Those connections should feed the semantic events above when the corresponding reconstructed gameplay systems can prove the event occurred.
