# Manual reconstruction: enemy death state

> Address convention: executable addresses from RevEngInstr use an image base
> `0x1000` above the shipped ELF VMA. Subtract `0x1000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | RevEngInstr entry | ELF entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `EnemyObject::isDead` | `0x002bf06e` | `0x002af06e` | 20 | 0 | 3 | 3 | 0 |
| `EnemyObject::setEnemyDead` | `0x002c1af8` | `0x002b1af8` | 9 | 3 | 1 | 0 | 1 |
| `EnemyObject::forceCurActionStateNoDead` | `0x002be332` | `0x002ae332` | 3 | 0 | 0 | 0 | 1 |

All three RevEngInstr rows are complete. The state setter called from
`setEnemyDead()` is a separate 2,615-op function whose corpus row is truncated.

## Death predicate and flag

`EnemyObject::isDead()` reads current health from `+0x164`. Positive or
unordered health returns false; so do states `0x0f` and `0x2c` from `+0x274`.
For all remaining cases, the result is the low byte at `+0x19d` XOR `1`.
The known setter writes only `0` or `1`, so the project boolean contract
returns true exactly when health is at most zero, the state is not exempt, and
the no-death byte is clear. The project predicate now uses `!(health <= 0)` for
its early false case, which also matches the native VFP comparison for NaN.

`EnemyObject::forceCurActionStateNoDead()` writes byte `1` to `+0x19d`; this
matches `enemy_health_state::set_force_action_state_no_dead(state, true)`.

## Death handoff boundary

`EnemyObject::setEnemyDead()` first stores float zero to `+0x164` and calls
`enemy_sendHarmHealthData()`. It then checks `isDead()`; only a true result
calls the large `setEnemyState()` routine, passing state code `0x15` and the
observed remaining arguments `1`, `1`, and `-1`. The existing clean-room
`enemy_health_state::set_dead()` mirrors the direct health write. The callback
and the effects inside the truncated state setter remain open, so this entry
does not claim to restore the full native death animation or notification.

The subsystem and function-role gold labels are in
`enemy-death-state-labels.jsonl`. Local RevEngInstr dataset v27 has 9,172
labelled functions, 6,880 structural groups, and 9,768 gold label rows (train
7,367, test 813, validation 992). Its corpus and generated dataset remain
outside Git.
