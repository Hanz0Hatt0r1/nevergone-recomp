# Manual reconstruction: player hit and combo counters

> Address convention: executable addresses from RevEngInstr use an image base
> `0x1000` above the shipped ELF VMA. Subtract `0x1000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::isComboHit` | `0x0032997c` | 16 | 0 | 1 | 3 | 0 |
| `MEPlayer::isHitFlag` | `0x00329996` | 16 | 0 | 1 | 3 | 0 |
| `MEPlayer::addHitFlagCount` | `0x003299b0` | 12 | 0 | 1 | 2 | 1 |
| `MEPlayer::addComboHitCount` | `0x003299c0` | 12 | 0 | 1 | 2 | 1 |
| `MEPlayer::tempHitFlagCount` | `0x003299d0` | 12 | 0 | 1 | 2 | 0 |
| `MEPlayer::tempComboHitCount` | `0x003299dc` | 12 | 0 | 1 | 2 | 0 |

All six RevEngInstr rows are complete and each function's Thumb body is short.
Those corpus metrics were used to rank the candidates; exact field behavior
comes from the instructions at these entries.

## Recovered fields and behavior

The methods use the action-system pointer at `MEPlayer +0x234`:

| Operation | Player snapshot | Action-system counter |
| --- | ---: | ---: |
| Hit window | `MEPlayer +0x2f4` | `ActionSystem +0x1fc` |
| Combo window | `MEPlayer +0x2f8` | `ActionSystem +0x1f8` |

For either gate, a null action-system pointer returns true. Otherwise the gate
returns whether the player's saved snapshot differs from the action-system
counter. The matching `add*Count()` method copies the current action-system
counter into the player's snapshot, but does nothing when the pointer is null.
The `temp*Count()` methods expose the action-system counter, or zero if there
is no action system.

`player_hit_window_contract` mirrors these two independent 32-bit counters and
their read/copy behavior. It does not infer how a counter changes, how many hit
points collide, or how damage is applied. Those operations remain in the
incomplete large `MEPlayer::isBeaten()` and `BattleManager::enemyHitPlayer()`
paths.

Gold subsystem and function-role labels for the six entries are recorded in
`player-hit-window-counters-labels.jsonl`. Adding them to the previous manual
restoration v12 labels and rebuilding as v13 produced 9,140 labelled
functions, 6,853 structural groups, and 9,701 gold label rows. The generated
corpus and dataset remain outside Git.
