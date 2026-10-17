# Manual reconstruction: enemy action hit-point selection

> Address convention: executable addresses from RevEngInstr use an image base
> `0x1000` above the shipped ELF VMA. Subtract `0x1000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `EnemyObject::curActionHitPointCount` | `0x002be9fc` | 8 | 1 | 1 | 1 | 0 |
| `EnemyObject::beatHitPoint` | `0x002bea08` | 36 | 2 | 3 | 6 | 2 |
| `EnemyActionsSystem::GetCurFrameHitPointCount` | `0x002bc196` | 51 | 3 | 5 | 8 | 1 |
| `EnemyActionsSystem::GetCurFrameHitPoint` | `0x002bc1e8` | 87 | 7 | 6 | 11 | 2 |

All four RevEngInstr rows are complete. Thumb assembly and Ghidra
decompilation establish these data paths:

1. `EnemyActionsSystem::GetCurFrameHitPointCount()` scans the Section-E array
   at `EnemyActionsData +0x84`. It compares each `ActionFrameData +0x74` with
   the current frame at `EnemyActionsSystem +0x294`, writes the zero-based
   matching ordinal to `ActionFrameData +0x54`, and returns the match count.
2. `EnemyActionsSystem::GetCurFrameHitPoint()` selects a Section-E record by
   that frame index and ordinal, then uses the same array index in the parallel
   sprite array at `EnemyActionsSystem +0x150`. It obtains the sprite's
   transformed `CCRect` through `CCSprite::sprBoundingBox()` and copies the
   rectangle origin to its `CCPoint` output.
3. `EnemyObject::curActionHitPointCount()` returns zero when its action-system
   pointer at `EnemyObject +0x10c` is null; otherwise it delegates to the
   count accessor above.
4. `EnemyObject::beatHitPoint()` returns false if there is no action system or
   the current frame has no hit-points. Otherwise it calls
   `GetCurFrameHitPoint()`, adds `EnemyObject +0x118` to the output x and
   `EnemyObject +0x284` to the output y, and returns true.

The Section-E parser now names its first serialized `int32` only as the frame
index at `ActionFrameData +0x74`. The shared `action_hitpoint_record_contract`
exposes frame-matching records for player and enemy documents. The enemy
combat-frame result includes those record references, and
`tutorial_attack_live_gate::live_resolve_hitpoint()` exposes the selected
ordinal and point translation behind the current wave-generation guard. Its
sprite-bounds origin and object offsets are inputs because the native Cocos
sprite objects have not been reconstructed yet.

This recovers frame/count/ordinal/index selection and the two enemy-coordinate
offsets. It does not create or position the native parallel sprites, derive
their transformed `CCRect` origins from the remaining Section-E scalars, or
implement the later player-point comparisons and distance rules in
`MEPlayer::isBeaten()` / `BattleManager::enemyHitPlayer()`. Damage, guard,
dodge, parry, and knockback remain outside this bounded slice.

Gold function-role labels for all four functions are recorded in
`enemy-action-hitpoint-selection-labels.jsonl`. The two `EnemyActionsSystem`
entries already had gold `combat` subsystem labels in the prior dataset, so
this addition avoids duplicate label targets. Adding six new gold rows to
manual-restoration v15 and rebuilding as v16 produced 9,150 labelled
functions, 6,860 structural groups, and 9,723 gold label rows. The generated
corpus and dataset remain outside Git.
