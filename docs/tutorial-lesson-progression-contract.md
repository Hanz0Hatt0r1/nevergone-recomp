# Tutorial LessonLayer progression contract

This note records the bounded clean-room progression gates recovered from the
original ARMv7 `LessonLayer::showDialogue()`, `nextLesson()` and
`nextLesson(MEPLAYERSTATE)` functions. It does not reproduce proprietary lesson
text, artwork, enemy data or game assets.

## Cursor semantics

`LessonLayer` stores its lesson cursor at object offset `+0x148` in the original
32-bit layout. `showDialogue()` selects the `LessonData` whose `getStep()` equals
the current cursor and then stores `step + 1` back into that cursor before the
player satisfies the newly shown lesson. Therefore the active interactive
prompts are observed by the completion callbacks with cursor values `4..12`.

Correlating that cursor range with the already recovered nine tutorial prompts
gives:

| Cursor | Active tutorial objective |
| ---: | --- |
| 4 | Run |
| 5 | Jump |
| 6 | Attack / eliminate current tutorial enemies |
| 7 | Block |
| 8 | Counter-attack |
| 9 | Dodge |
| 10 | Air Attack |
| 11 | Fill Rage with the tutorial quick-use item |
| 12 | Execution / clear the final enemies |

The project-owned `tutorial_lesson_progression_contract` stores these as bounded
semantic labels only; it does not claim that the cursor value is the enum value
stored inside every original `LessonData` object.

## `nextLesson(MEPLAYERSTATE)` gates

The recovered state-callback gates are:

| Cursor | Native gate | Progression side effect retained by the contract |
| ---: | --- | --- |
| 4 | `MEPLAYERSTATE == 0x01` | advance |
| 5 | `MEPLAYERSTATE == 0x20` | advance; start the enemy-id-3/state-`0x1c` attack wave |
| 7 | `MEPLAYERSTATE == 0x24` | advance; arm the following counter gate |
| 8 | state `0x09` for player index 1, otherwise `0x08` | advance |
| 9 | state in `0x11, 0x17, 0x2f, 0x30` | advance; create the later air-attack target and grant the tutorial Rage item |
| 11 | `ToolsQuickUseUI +0x10c != 0` | clear the flag, advance and fill super energy |
| 12 | live-enemy count is zero | advance, reset controls and schedule `secondDlgCallBack` after exactly 2.0 seconds |

Counters 6 and 10 are not advanced by those raw player-state cases; their
completion is observed through the no-argument `nextLesson()` path described
below.

## `nextLesson()` world-state gates

For cursor 6, the original first requires `BattleManager::enemyNullCount()==0`
and then accepts player field `+0x278` only when it is `0x1b` or `0x1c`. On
success it advances and creates the single enemy used by the following block
lesson (enemy id 3, state `0x2f`).

For cursor 10, zero live enemies advances the lesson and constructs the next
multi-enemy wave used by the later tutorial sequence. The recovered wave uses
enemy id 3 / state `0x1c`; exact dynamic positions remain a separate geometry
reconstruction because they depend on player facing and scene bounds.

The no-argument cursor-12 path also touches enemy pause state, but does not by
itself complete the lesson; the evidence-backed terminal completion remains the
zero-live-enemy gate in `nextLesson(MEPLAYERSTATE)`.

## `enemyNullCount()` semantics

The original name is misleading. `BattleManager::enemyNullCount()` iterates the
manager's enemy `CCArray`, calls `EnemyObject::isDead()` for every entry, and
increments its result only when `isDead()` returns false. It therefore behaves
as a **live-enemy count** for the tutorial gates. Zero means that no tracked
EnemyObject remains alive.

The clean-room helper `enemy_null_count_from_dead_flags()` reproduces only this
counting property. It deliberately does not attempt to reconstruct the complete
`EnemyObject::isDead()` object-layout predicate.

## Current integration boundary

This contract does not turn a raw A/B/C press into a combat success. The current
runtime already has live Run and Jump input integration, while Attack and later
steps require genuine gameplay outcomes. The contract gives those future
systems exact acceptance gates and side-effect boundaries:

- enemy roster/death code can publish the zero-live-enemy transitions without
  guessing tutorial semantics;
- combat state can map recovered player-state transitions for Block,
  Counter-attack and Dodge;
- item/runtime UI can feed the exact quick-use gate;
- the final zero-live-enemy transition can start guide dialogue 9 only through
  the recovered 2.0-second scheduler boundary.

Until those systems exist, unsupported outcomes remain fail-closed rather than
being inferred from button presses.
