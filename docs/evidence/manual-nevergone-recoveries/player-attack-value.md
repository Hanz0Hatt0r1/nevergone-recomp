# Manual reconstruction: player attack value

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::getAttack` | `0x0032c6f8` | 31 | 4 | 0 | 6 | 0 |
| `MEPlayer::setEquipAddAttack` | `0x0032a5f6` | 4 | 0 | 0 | 0 | 1 |
| `PlayerStatusInfo::getLevelAttack` | `0x00327236` | 5 | 0 | 0 | 1 | 0 |

All three rows are complete. The original decompilation and ARM instructions
establish this order for `MEPlayer::getAttack()`:

1. `MEEquip::equipAtIdx(..., 0)` selects weapon-array entry zero from the
   `MEEquip` object held at `DataManager +0x24`; the array pointer is at
   `MEEquip +0x104`.
2. Load the weapon's base attack at `WeaponsItemData +0x38`.
3. Add `WeaponsItemData::getFinalAddBase_atk()`.
4. Add `PlayerStatusInfo::getLevelAttack()`, which reads `+0x110` from the
   `PlayerStatusInfo` object at `DataManager +0x48`.
5. Add `MEPlayer +0x114`, written by `MEPlayer::setEquipAddAttack()`.
6. Add the separate float at `PlayerStatusInfo +0x144`.

`player_combat_stat_contract::resolve_attack()` preserves that left-to-right
float sum. `set_equipment_add_attack()` stores the setter's value in its
project-owned state field corresponding to `MEPlayer +0x114`. The term at
`PlayerStatusInfo +0x144` is kept structurally named because its independent
producer has not been identified.

`WeaponsItemData::getFinalAddBase_atk()` has a complete 59-operation row. Its
decompilation shows that it gates the hole bonus on `ManagementLayer +0x744`,
gates the temper bonus on `WeaponsItemData +0x18 == 0`, then adds the extra
base-attack result and `WeaponsItemData +0x60`; it also updates byte `+0x64`
and floors a negative result to its initial constant. Its
`WeaponsItemData::getExtraBase_atk()` helper has 176 operations, and the initial
constant has not yet been verified, so the
clean-room attack sum accepts this getter's result as an input rather than
guessing those remaining details. The native `getAttack()` has no visible
empty-weapon fallback after `equipAtIdx()`.

The three gold subsystem and function-role labels are recorded in
`player-attack-value-labels.jsonl`. The local dataset v18 contains 9,154
labelled functions, 6,862 structural groups, and 9,731 gold label rows after
adding the six rows to manual-restoration v17. The generated corpus and
dataset remain outside Git.
