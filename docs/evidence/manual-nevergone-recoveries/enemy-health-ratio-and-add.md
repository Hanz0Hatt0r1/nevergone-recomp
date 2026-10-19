# Manual reconstruction: enemy health ratio and healing

> Address convention: executable addresses from RevEngInstr use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Functions

| Function | RevEngInstr entry | ELF entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `EnemyObject::enemyHealthRatio` | `0x002be308` | `0x002ae308` | 11 | 0 | 0 | 2 | 0 |
| `EnemyObject::addHealth` | `0x002bfa24` | `0x002afa24` | 8 | 0 | 0 | 1 | 1 |

`enemyHealthRatio()` reads maximum health from `+0x160` and current health
from `+0x164`, computes `current / max * 100`, and returns the result. The
original has no zero-max guard; the clean-room model keeps its existing
defensive return of zero for invalid state, while roster initialization
requires positive base health.

`addHealth(delta)` loads current health, adds `delta`, and stores the sum back
to `+0x164`. It does not clamp. `enemy_health_state::health_ratio_percent()`
and `enemy_health_state::add_health()` implement those contracts.

The subsystem and function-role gold labels are in
`enemy-health-ratio-and-add-labels.jsonl`. Local RevEngInstr dataset v28 has
9,174 labelled functions, 6,882 structural groups, and 9,772 gold label rows
(train 7,369, test 813, validation 992). Its corpus and split files remain
outside Git.
