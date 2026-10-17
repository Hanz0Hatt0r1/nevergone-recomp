# Manual reconstruction: player action hit-point record selection

> Address convention: executable addresses from RevEngInstr use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::curActionHitPointCount` | `0x00328846` | 5 | 1 | 0 | 1 | 0 |
| `MeActionsSystem::curFrameHitPointCount` | `0x00338f64` | 37 | 2 | 2 | 6 | 1 |
| `MeActionsSystem::curFrameHitPoint` | `0x00338fa0` | 80 | 7 | 4 | 10 | 2 |

All three RevEngInstr rows are complete. Thumb disassembly and Ghidra
decompilation confirm the following data path:

1. `MeActionsData::loadWBGFileWithHPData()` reads Section E's signed count and
   fixed 53-byte records, stores the first serialized `int32` at
   `ActionFrameData +0x74`, and appends each record to `MeActionsData +0xc8`.
2. `MeActionsSystem::curFrameHitPointCount()` walks that `+0xc8` array. It
   compares `ActionFrameData +0x74` with `MeActionsSystem +0x1a4`, stores the
   zero-based matching ordinal at `ActionFrameData +0x54`, and returns the
   number of records for the current frame.
3. `MeActionsSystem::curFrameHitPoint()` matches both fields and uses the
   selected array index to fetch a parallel object from `MeActionsSystem
   +0x198`, sets its position, and obtains its transformed `CCRect` through
   `CCSprite::sprBoundingBox()`. The method copies the rectangle origin into a
   `CCPoint`; `MEPlayer::curActionHitPointCount()` delegates to the active
   system at `MEPlayer +0x234`.

The Sections A-G document parser already retains the same Section-E records
for player action files. `action_hitpoint_record_contract` exposes the current
frame's source record indices and matching ordinals. The player's frame-lookup
result carries these references alongside the existing sprite frame lookup.

This restores the data-backed count/ordinal/index selection stage. It does not
create the native `+0x198` sprite objects, reproduce their position updates,
calculate the transformed `sprBoundingBox()` origin, or perform the later
point comparisons. `MEPlayer::beatHitPoint()` now has a separate bounded
contract for its failure checks and `+0x268/+0x26c` player-coordinate
translation. Native sprite creation/positioning and the later comparisons
remain unresolved in the large `BattleManager::{playerHitEnemy,enemyHitPlayer}`
and `MEPlayer::isBeaten()` routines.

Gold subsystem and function-role labels for the three complete entries are
recorded in `player-action-hitpoint-record-selection-labels.jsonl`. Adding
them to the previous manual-restoration v14 labels and rebuilding as v15
produced 9,148 labelled functions, 6,858 structural groups, and 9,717 gold
label rows. The generated corpus and dataset remain outside Git.
