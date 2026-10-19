# Manual reconstruction: enemy object action movement setter

> Address convention: RevEngInstr executable addresses use an image base
> `0x10000` above the shipped ELF VMA. Gold-label `source_entry` keeps that
> corpus address; subtract `0x10000` to inspect the ARMv7 ELF.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Recovered setter paths

`EnemyObject::setEnemyMove(CCPoint,bool)` begins at image address `0x002bf212`
and ELF VMA `0x002af212`. Thumb instructions at `0x2af222..0x2af23a` return
before modifying the movement point when
`(EnemyObject+0x274 & ~0x08) == 0x14`, or when the state equals `0x2e` or
`0x36`. The masked predicate includes both states `0x14` and `0x1c`.

For the remaining states, the incoming movement point is added to
`EnemyObject+0x280` before the subsequent scene-edge adjustment. The
`isEnemyMoveState()` branch always passes the updated `+0x280` point to the
action system's virtual position setter. When `EnemyActionsSystem+0x2b6` is
also set, the method writes `EnemyObject+0x118` from the updated `+0x280` plus
the point after the scene-edge adjustment. The common tail copies `+0x280` to
`+0x288` and refreshes `enemyHLPos()`.

`enemy_object_set_enemy_move::apply()` models the early-return states,
`+0x280` accumulation, the observed move-state action-system position call,
the conditional `+0x118` write, and the common `+0x288` copy. The
non-move-state event/swap-buffer path is deliberately marked incomplete because
it depends on event-scene objects and the setter's boolean argument.

## Caller ordering and live-render boundary

`EnemyObject::tickEnemy()` calls `EnemyActionsSystem::GetCurMoveValue()` and
then `setEnemyMove(point, false)` before stepping the action system. The
supported tutorial enemy state `0x1c` matches the setter's early-return mask,
so that state's action displacement must not be applied to `+0x280`. The
runtime render queue currently uses the spawn point; movement for other enemy
states still needs a complete frame-history and action-state transition model
before it can safely alter rendered positions.

`enemy-object-set-move-labels.jsonl` adds the gold function-role label. The
binary and generated RevEngInstr datasets remain outside Git.
