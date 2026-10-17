# Manual reconstruction: enemy physical damage states

> Address convention: RevEngInstr entries are `0x10000` above the shipped ELF
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
| `EnemyPhysical::initEnemyPhysical(CCString*, int)` | `0x002c6178` | `0x002b6178` | 10 | 1 | 0 | 0 | 1 |
| `EnemyPhysical::loadPhysicalConfig` | `0x002c6000` | `0x002b6000` | 574* | 35 | 4 | 6 | 10 |

\* The RevEngInstr row emits 256 of 574 operations and is marked truncated.

The state-machine and copy rows are complete. Their Ghidra pseudocode is retained in
the local Never Gone decompilation export; the `updatePhysicalState` Thumb body
and its `100.0f` literal independently confirm the state offsets and percentage
calculation. `loadPhysicalConfig` is truncated in RevEngInstr, but the saved
Ghidra pseudocode and ELF string literals recover its configuration fields.

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
| `+0x30` | Integer percentages for state IDs 1 through 3 |
| `+0x3c` | Current state index |

## Recovered configuration contract

The Thumb literal references resolve to these dictionary keys in the shipped
ELF's `.rodata`: `noPhysical`, `startPhysical`, `maxPhysical`, and
`physicalDamage_01` through `physicalDamage_04`. The saved Ghidra pseudocode
maps them to byte `+0x14`, initial index `+0x24`, last index `+0x28`, and four
integer percentages at `+0x2c` through `+0x38`.

`initEnemyPhysical(CCString*, int)` converts its integer duration to float,
writes `+0x18`, then calls `loadPhysicalConfig()`. When the dictionary exists,
the loader reads `noPhysical`. If true, it marks the object disabled and sets
the current index to zero. If false, it reads the start and max indices and
four percentages, sets current index to the start index, and computes the
initial threshold as `damagePercent[start] / 100 * duration`. A missing
dictionary returns false. The clean-room `Config`/`initialize()` model accepts
those already parsed fields and keeps file lookup as an input boundary.

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
and copy routine. It accepts parsed values and duration from the caller; it
does not fabricate enemy-specific inputs. The corpus row for
`loadPhysicalConfig()` remains truncated, so this reconstruction does not
claim the full Cocos file-resolution and dictionary cleanup path. `Roster`
accepts this configuration per materialized enemy, applies the recovered
online-health damage amount to the physical sequence, and exposes explicit
damage advancement and next-state transitions for other recovered hit paths.

Gold labels are in `enemy-physical-damage-state-labels.jsonl`.
The rebuilt local RevEngInstr dataset v32 has 9,181 labelled functions,
6,889 structural groups, and 9,786 gold-label rows (train 7,375, test 814,
validation 992). Generated corpus and splits remain outside Git.
