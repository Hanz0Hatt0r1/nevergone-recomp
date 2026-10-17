# Manual reconstruction: player health and damage-absorption modifiers

> Address convention: executable addresses from RevEngInstr use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::setToolsDamageAbsorb` | `0x0032a572` | 5 | 0 | 0 | 0 | 1 |
| `MEPlayer::setEquipAddHealth` | `0x0032a5fc` | 4 | 0 | 0 | 0 | 1 |
| `MEPlayer::setEquipAddDamageAbosr` | `0x0032a602` | 4 | 0 | 0 | 0 | 1 |
| `MEPlayer::addEquipAddHealth` | `0x0032a60e` | 8 | 0 | 0 | 1 | 1 |
| `MEPlayer::setToolsAddHealth` | `0x0032bf9e` | 4 | 0 | 0 | 0 | 1 |

All five RevEngInstr rows are complete, straight-line functions. The original
Thumb instructions confirm their writes:

- `setEquipAddHealth()` copies the float argument to `MEPlayer +0x110`.
- `addEquipAddHealth()` loads `+0x110`, adds the float argument, and stores it
  back.
- The native symbol `setEquipAddDamageAbosr()` copies the argument to
  `MEPlayer +0x118` (the source symbol contains this spelling).
- `setToolsAddHealth()` copies the argument to `MEPlayer +0x120`.
- `setToolsDamageAbsorb()` computes `1.0f - argument` and stores it at
  `MEPlayer +0x430`.

`player_combat_stat_contract` mirrors these writes and arithmetic. Its later
bounded `MEPlayer::setEquipAddAttack()` and `MEPlayer::getAttack()` contracts
are documented in `player-attack-value.md`. The health and absorption
modifiers still are not combined into an effective maximum or total damage
absorption: `MEPlayer::getMaxHealth()` and `getAllDamageAbsorb()` have
truncated corpus rows, so those formulas need separate recovery.

Gold subsystem and function-role labels for these five functions are recorded
in `player-combat-stat-writes-labels.jsonl`. Adding them to the previous manual
restoration v13 labels and rebuilding as v14 produced 9,145 labelled
functions, 6,855 structural groups, and 9,711 gold label rows. The generated
corpus and dataset remain outside Git.
