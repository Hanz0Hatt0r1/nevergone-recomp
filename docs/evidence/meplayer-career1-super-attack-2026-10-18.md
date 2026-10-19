# Career 1 tutorial super attack — 2026-10-18

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Native route

`Controls::tick()` synthesizes function state `0x1c` while A and C are both
pressed ([Controls state evidence](controls-function-state-tick-2026-10-12.md)).
At ELF `0x00321098`, `MEPlayer::setFunctionState()` handles that state only
after the common state gates. The branch at `0x00321170` reads the player's Rage
at `MEPlayer +0x328`; below `100.0f` it returns without requesting an action.
At or above 100 it calls `setActionState(0x3a, true)` at `0x00321190`.

On completion, `MEPlayer::actionState()` at ELF `0x0031e43c` advances `0x3a` to
`0x3b`, then `0x3b` to `0x3c`. For Career 1 (`MEPlayer +0x144 == 1`), completed
`0x3c` returns to idle state `0x1b`. The native calls use the `true` secondary
argument for each transition in this chain.

## Imported Career 1 action data

The user-owned imported `Player01_Res/p01_actions.plist` has SHA-256
`f0cfba7dba54a17c055d1894823a103956220f0f9aaf4c71c25833aa5b96c514`. Its
records map action keys 59, 60, and 61 to these non-looping documents:

| MEPLAYERSTATE | Action key | Imported action document |
| ---: | ---: | --- |
| `0x3a` | 59 | `p01_ss01a.actData` |
| `0x3b` | 60 | `p01_ss01b_a.actData` |
| `0x3c` | 61 | `p01_ss01c.actData` |

The state-to-action-key offset is already used by the recovered Career 1
directional actions. All 53 plist action documents are present in the 125-entry
Career 1 action-data bundle, as recorded in
[`player-actionsdata-container`](player-actionsdata-container-2026-10-12.md).
Neither the plist nor proprietary action documents are committed.

## Reconstructed tutorial path

The native tutorial's cursor-11 callback advances only after
`ToolsQuickUseUI +0x10c` reports a successful quick-use action, and that branch
fills super energy ([lesson progression evidence](../tutorial-lesson-progression-contract.md)).
The reconstructed tutorial has no item inventory, so a tap on quick-use slot 0
while the interactive tutorial is at **Fill Rage** represents that granted
potion use and advances the prompt to **Execution**. This is a project-owned
mapping for the single tutorial potion; it is not a claim that every quick-use
slot always succeeds.

During **Execution**, A+C produces function state `0x1c`. The live bridge accepts
it only for Career 1, from idle state `0x1b`, and after the tutorial Rage-fill
event. It then loads the three imported animations and follows the recovered
completion chain back to `0x1b`.

This increment restores the tutorial animation route. It does not model generic
Rage accumulation, item inventory/consumption, the native screen-blend and tool
effects, or the final enemy kills required to complete the Execution lesson.
Damage application remains separately blocked by the unresolved weapon/status
attack inputs in
[`player-damage-core.md`](manual-nevergone-recoveries/player-damage-core.md#runtime-data-boundary-checked-on-device).

Gold labels for the native dispatch and completion functions are recorded in
[`player-super-attack-labels.jsonl`](manual-nevergone-recoveries/player-super-attack-labels.jsonl).
