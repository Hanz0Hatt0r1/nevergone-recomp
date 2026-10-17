# Manual reconstruction: enemy attack-miss frame gate

> Address convention: executable addresses from RevEngInstr use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `EnemyObject::isAttackMiss` | `0x002bef2c` | 70 | 0 | 9 | 9 | 0 |

The complete RevEngInstr row and Thumb assembly show a state-specific frame
cutoff. The enemy state comes from `EnemyObject +0x274`; the frame comes from
the action system at `EnemyObject +0x10c`, offset `+0x294`:

| `EnemyObject +0x274` | Frame cutoff | Returns true when |
| --- | ---: | --- |
| `0x07`, `0x08` | 4 | current frame `> 4` |
| `0x09`, `0x0a` | 10 | current frame `> 10` |
| `0x0b` | 15 | current frame `> 15` |
| `0x0c`, `0x11`, `0x2d` | 21 | current frame `> 21` |
| any other value | — | never |

Each recognized branch compares the current frame with its cutoff. The final
`ITE LE` returns false when the frame is at or below the cutoff and true only
after it. The function does not guard a null action-system pointer before
reading `+0x294`.

`enemy_hit_gate_contract::is_attack_miss()` mirrors this decision using the
two recovered integer fields. It does not model the caller's action-system
lifetime; callers must supply the current frame from a valid action system.

The gold subsystem and function-role labels are recorded in
`enemy-attack-miss-window-labels.jsonl`. The local dataset v21 contains 9,159
labelled functions, 6,867 structural groups, and 9,741 gold label rows after
adding two new labels to manual-restoration v20. The generated corpus and
dataset remain outside Git.
