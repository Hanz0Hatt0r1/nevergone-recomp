# Manual reconstruction: player-to-enemy intersection

> Address convention: RevEngInstr executable addresses use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `BattleManager::playerIntersectsEnemy` | `0x002b69e4` | 255 | 16 | 17 | 11 | 8 |

The RevEngInstr row is complete. Thumb disassembly confirms the enemy scan,
filters, distance gates, first-match behavior, and writes to the battle-manager
intersection fields. The full routine also calls `EnemyObject::enemyHLPos()`
for its state-dependent position refresh and reads `enemyOrgHLPos()` from
`EnemyObject +0x184` for the intersection decision.

## Recovered decision

Each call clears `BattleManager +0x1d4`, `+0x1e0`, and `+0x1e1`. The candidate
point is `MEPlayer::playerHLPoint()` (`MEPlayer +0x130`) with the X component of
the input point added; its Y component is ignored. The routine walks the enemy
array at `BattleManager +0x104` in order and skips enemies that are dead, in
state `0x27` or `0x24`, have a nonzero byte at `EnemyObject +0x31f`, or satisfy
`EnemyObject::isFly()`.

For each remaining enemy, it accepts the first one whose origin hit point is
strictly less than 80 units from the candidate and whose absolute vertical
difference is at most 30. The resulting boolean is stored at `+0x1d4`; the
enemy origin point is copied to `+0x1d8`. For player face direction `0`, the
routine sets `+0x1e1` and moves the target X by `+80` when the player point is
to the right of the enemy point, otherwise `-80`. For face direction `1`, it
sets `+0x1e0` and uses `-80` when the player point is to the left of the enemy
point, otherwise `+80`. Other face-direction values keep the origin point
unchanged.

`battle_manager_player_intersection_contract::resolve()` reproduces the
selection and side-effect values from explicit, already-resolved points and
enemy predicates. It intentionally does not synthesize native `CCArray`,
`EnemyObject`, or sprite-bound state; those inputs still need to be connected
to the live scene before this contract can drive the gameplay frame loop.

The function-role gold label is recorded in
`battle-manager-player-intersection-labels.jsonl`; the existing subsystem label
already classifies `BattleManager` as combat. The binary and generated
RevEngInstr corpus remain outside Git. Local manual-restoration dataset v44 now
has 9,195 labelled functions, 6,903 structural groups, and 9,816 gold label
rows (train 7,385; test 818; validation 992).
