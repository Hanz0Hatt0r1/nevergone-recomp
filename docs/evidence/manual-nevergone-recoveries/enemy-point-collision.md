# Manual reconstruction: enemy point collision

> Address convention: executable addresses from RevEngInstr use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `EnemyObject::isBeaten` | `0x002bf612` | 55 | 3 | 5 | 5 | 0 |
| `EnemyObject::isEnemyMoveState` | `0x002bef08` | 24 | 0 | 5 | 2 | 0 |
| `EnemyObject::enemyBoundingBox` | `0x002bf09c` | 26 | 8 | 2 | 1 | 0 |
| `EnemyActionsSystem::actionBoundingBox` | `0x002bbf08` | 14 | 2 | 3 | 2 | 0 |

Both corpus rows are complete. Thumb instructions and Ghidra decompilation
confirm the following decision and side effect.

## Point test

`EnemyObject::isBeaten(point)` first copies the input `CCPoint` into
`EnemyObject +0x314`, including when the function later returns false. It then
returns false if the action-system pointer at `+0x10c` is null or if the byte
at `EnemyActionsSystem +0x2b5` is nonzero. It also returns false for the
specific combination `EnemyObject +0x248 != 0`, `+0x2a0 == 0x1a`, and state
`+0x274 == 0x14`.

For remaining cases, it calls `enemyBoundingBox()` and tests the point using
`cocos2d::CCRect::containsPoint()`. The original rectangle test includes its
minimum and maximum edges (`origin` through `origin + size`). The new
`enemy_collision_contract::is_beaten()` mirrors these gates and returns the
saved point alongside the collision result. It takes the native action sprite
bounds as input because recreating the live `CCSprite::sprBoundingBox()` is
outside this gate.

## Bounding rectangle

`EnemyActionsSystem::actionBoundingBox()` returns the action sprite's
`sprBoundingBox()` when byte `+0x26c` is nonzero and pointer `+0x10c` is
non-null. Otherwise it returns a 1-by-1 rectangle at the origin, confirmed by
the constructor arguments in Thumb. `EnemyObject::enemyBoundingBox()` returns
the same fallback when the action-system pointer at `+0x10c` is null. With an
action system, it gets the action bounding rectangle and translates only its
origin: by enemy fields `+0x280` / `+0x284` when `isEnemyMoveState()` is true,
or `+0x118` / `+0x11c` otherwise. Width and height are preserved.

`enemy_collision_contract::enemy_bounding_box()` reproduces these fallback
and translation rules from the sprite rectangle and recovered fields.

## Movement-state helper

`EnemyObject::isEnemyMoveState(state)` returns true for states `0`, `1`,
`0x1b`, `0x2a`, `0x2b`, and `0x35`. For any other state it returns false if
the action-system pointer at `+0x10c` is null; otherwise it returns the byte at
`EnemyActionsSystem +0x2b6` as a boolean. The compact gate is in
`enemy_hit_gate_contract::is_enemy_move_state()`.

The new subsystem and function-role gold labels are recorded in
`enemy-point-collision-labels.jsonl`; `EnemyActionsSystem::actionBoundingBox`
reuses its existing `combat` subsystem label and adds the function-role label
there. The local RevEngInstr manual-restoration
dataset v24 has 9,165 labelled functions, 6,873 structural groups, and 9,754
gold label rows (train 7,364, test 811, validation 990). The corpus and
generated dataset remain outside Git.
