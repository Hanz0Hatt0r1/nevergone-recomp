# Manual reconstruction: enemy beaten-state gates

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `EnemyObject::setForceBeatenState` | `0x002bea92` | 7 | 0 | 0 | 0 | 2 |
| `EnemyObject::isBeatenState` | `0x002bef88` | 27 | 0 | 6 | 2 | 0 |
| `EnemyObject::checkInputStateIsBeaten` | `0x002befc0` | 23 | 0 | 6 | 1 | 0 |

All three RevEngInstr rows are complete and small enough to reconstruct. The
field meanings below are taken from the original Thumb instructions and the
matching Ghidra decompilation, not inferred from the corpus ranking.

## State gate

`EnemyObject::isBeatenState()` reads candidate state from `+0x274` and a byte
from `+0x22a`. `checkInputStateIsBeaten(state)` applies the same comparisons to
its argument. Both return true for states `5..12`, `15..17`, `0x2c`, `0x2d`,
and `0x34`; every other state returns false. When the byte at `+0x22a` is
nonzero, states `5` and `6` return false. The original unsigned subtract-and-
compare sequence is preserved in `enemy_hit_gate_contract`, including 32-bit
wraparound for values below each range.

The two helpers are therefore a current-state gate and an input-state gate
over the same predicate. This does not recover the caller's full state
transition or damage path.

## Paired state writes

`EnemyObject::setForceBeatenState(bool)` performs two byte stores: it writes
the bool parameter to `+0x19c`, then to `+0x19d`. The project contract returns
these paired byte values without assigning additional meaning to either
field.

The subsystem and function-role gold labels are recorded in
`enemy-beaten-state-gates-labels.jsonl`. Local RevEngInstr dataset v22 now has
9,162 labelled functions, 6,870 structural groups, and 9,747 gold label rows
(train 7,361, test 811, validation 990). The corpus and generated dataset
remain outside Git.
