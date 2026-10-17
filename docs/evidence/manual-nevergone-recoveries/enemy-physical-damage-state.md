# Manual reconstruction: enemy physical damage states

> Address convention: RevEngInstr entries are `0x1000` above the shipped ELF
> VMA. Gold labels retain the RevEngInstr entry so they join the corpus.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Function evidence

| Function | RevEngInstr entry | ELF entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `EnemyPhysical::initEnemyPhysical(EnemyPhysical*)` | `0x002c5efc` | `0x002b5efc` | 66 | 0 | 0 | 11 | 11 |
| `EnemyPhysical::physicalStateDamageWithID` | `0x002c6188` | `0x002b6188` | 7 | 0 | 0 | 1 | 0 |
| `EnemyPhysical::updatePhysicalState` | `0x002c6190` | `0x002b6190` | 90 | 0 | 6 | 8 | 5 |
| `EnemyPhysical::nextState` | `0x002c6200` | `0x002b6200` | 43 | 0 | 2 | 5 | 3 |

The three RevEngInstr rows are complete. Their Ghidra pseudocode is retained in
the local Never Gone decompilation export; the `updatePhysicalState` Thumb body
and its `100.0f` literal independently confirm the state offsets and percentage
calculation.

## Recovered state contract

| Offset | Meaning |
| --- | --- |
| `+0x14` | Disabled byte |
| `+0x18` | Damage-state duration |
| `+0x1c` | Accumulated damage toward the next threshold |
| `+0x20` | Current damage threshold |
| `+0x24` | Initial state index copied with the config |
| `+0x28` | Last configured state index |
| `+0x2c` | Integer percentage for state ID 0 |
| `+0x30` | Integer percentage table for state IDs 1 onward |
| `+0x3c` | Current state index |

`physicalStateDamageWithID(id)` reads the configured integer at `+0x2c + 4*id`.
`updatePhysicalState(damage)` returns false while disabled or while the current
index is 2. Otherwise it adds damage to `+0x1c`. If the sum is below `+0x20`,
it retains the sum and returns false. When the threshold is reached, it clears
the accumulator. An unordered float comparison (a NaN accumulated value or
threshold) returns false and preserves the accumulator, matching the native
ARM comparison. If the current index is above the last configured index, it
returns false. If current equals last, it wraps to state 0; otherwise it
advances by one. The selected percentage divided by 100 and multiplied by the
duration becomes the next threshold, and the function returns true.

`nextState()` is the explicit transition path: when enabled, it first clears
the damage accumulator. If current is below last, it advances one state and
recomputes the threshold from that state's configured percentage. At or beyond
last, it leaves the state index and threshold unchanged.

The pointer overload of `initEnemyPhysical()` copies the disabled flag and
every 32-bit field from `+0x18` through `+0x3c`, including the initial index,
last index, four percentage values, and current index. `Roster`'s configuration
handoff uses the same value-copy behavior.

The state model retains the four percentage values present in the constructor
and copy routine. It accepts the selected duration and configuration values
from the caller because the 574-operation `loadPhysicalConfig()` corpus row is
truncated. It does not fabricate enemy-specific durations or percentages.
`Roster` accepts that physical configuration per materialized enemy, applies
the recovered online-health damage amount to the physical sequence, and exposes
explicit damage advancement and next-state transitions for other recovered hit
paths.

Gold labels are in `enemy-physical-damage-state-labels.jsonl`.
The rebuilt local RevEngInstr dataset v31 has 9,179 labelled functions,
6,887 structural groups, and 9,782 gold-label rows (train 7,374, test 813,
validation 992). Generated corpus and splits remain outside Git.
