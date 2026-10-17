# Manual reconstruction: player action hit-point origin translation

> Address convention: executable addresses from RevEngInstr use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::beatHitPoint` | `0x0032884e` | 43 | 5 | 3 | 2 | 0 |

The RevEngInstr row is complete. Thumb assembly and Ghidra decompilation show
the player-side wrapper around the already reconstructed Section-E selection:

1. `MEPlayer::beatHitPoint(int ordinal, CCPoint* output)` reads the active
   `MeActionsSystem*` from `MEPlayer +0x234` and calls
   `MeActionsSystem::curFrameHitPointCount()`. A zero count returns false.
2. It calls `MeActionsSystem::curFrameHitPoint()` with the output point and
   requested ordinal. If that accessor fails to find the frame/ordinal record
   or its parallel sprite, `beatHitPoint()` returns false.
3. On success, the accessor has supplied the transformed
   `CCSprite::sprBoundingBox()` rectangle origin. `beatHitPoint()` adds the
   floats at `MEPlayer +0x268` and `MEPlayer +0x26c` to x and y respectively,
   writes the resulting `CCPoint` to the caller, and returns true.

`player_hitpoint_contract::resolve()` mirrors the record and ordinal lookup
using `action_hitpoint_record_contract`, fails closed when the sprite bounds
origin or selected record is absent, and returns the translated point with its
Section-E record reference. The transformed origin is supplied as an input
because the native `MeActionsSystem +0x198` sprite array is not yet recreated.

This restores the player wrapper's success gates and coordinate translation.
It does not create the parallel sprites, derive their transformed rectangle
origins from the remaining Section-E floats, or reproduce point comparisons
and distance rules in `MEPlayer::isBeaten()` and
`BattleManager::playerHitEnemy()`.

The gold subsystem and function-role labels are recorded in
`player-hitpoint-origin-translation-labels.jsonl`. The local RevEngInstr
dataset v17 contains 9,151 labelled functions, 6,861 structural groups, and
9,725 gold label rows after adding these two labels to manual-restoration v16.
The generated corpus and dataset remain outside Git.
