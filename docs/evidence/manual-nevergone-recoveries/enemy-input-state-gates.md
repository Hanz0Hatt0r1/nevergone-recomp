# Manual reconstruction: enemy input-state gates

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `EnemyObject::checkInputStateIsSkill` | `0x002beff2` | 10 | 0 | 2 | 0 | 0 |
| `EnemyObject::checkInputStateIsAttack` | `0x002bf00c` | 23 | 0 | 6 | 0 | 0 |
| `EnemyObject::checkInputStateBeFlyAndBeDown` | `0x002bf048` | 15 | 0 | 4 | 0 | 0 |

All three rows are complete. Ghidra decompilation shows they inspect only the
input state parameter; the Thumb-level comparisons use unsigned 32-bit
subtractions and bounds checks.

## Accepted state values

The recovered predicates return true for these exact sets and false for all
other 32-bit values:

| Predicate | Accepted input state values |
| --- | --- |
| `checkInputStateIsSkill` | `0x1a`, `0x1c..0x29`, `0x2e` |
| `checkInputStateIsAttack` | `2..4`, `0x0d..0x0e`, `0x12..0x13`, `0x16`, `0x18..0x1a`, `0x1d..0x23`, `0x25..0x29` |
| `checkInputStateBeFlyAndBeDown` | `0x09..0x0c`, `0x0f`, `0x11`, `0x2c..0x2d`, `0x34` |

`enemy_hit_gate_contract` retains the original unsigned arithmetic, so values
below a range wrap as ARM32 does. These input classifiers complement the
recovered enemy beaten-state gate without assigning behavior to their callers.

The subsystem and function-role gold labels are in
`enemy-input-state-gates-labels.jsonl`. The local RevEngInstr manual-
restoration dataset v25 now has 9,168 labelled functions, 6,876 structural
groups, and 9,760 gold label rows (train 7,367, test 811, validation 990). The
corpus and generated dataset remain outside Git.
