# Manual reconstruction: player attack value

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::getAttack` | `0x0032c6f8` | 31 | 4 | 0 | 6 | 0 |
| `MEPlayer::setEquipAddAttack` | `0x0032a5f6` | 4 | 0 | 0 | 0 | 1 |
| `PlayerStatusInfo::getLevelAttack` | `0x00327236` | 5 | 0 | 0 | 1 | 0 |
| `WeaponsItemData::getFinalAddBase_atk` | `0x00345850` | 59 | 4 | 5 | 3 | 2 |
| `WeaponsItemData::getStarLevel` | `0x0034426c` | 15 | 2 | 1 | 2 | 0 |
| `WeaponsItemData::getAddHoleBase_atk` | `0x00345054` | 29 | 1 | 1 | 1 | 0 |
| `WeaponsItemData::getAddTemperBase_atk` | `0x00344f24` | 15 | 0 | 0 | 2 | 0 |

All seven rows are complete. The original decompilation and ARM instructions
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

`WeaponsItemData::getFinalAddBase_atk()` gates the hole contribution on a
nonzero byte at `ManagementLayer +0x744`, gates the temper contribution on
`WeaponsItemData +0x18 == 0`, then adds the result from
`getExtraBase_atk()` and `WeaponsItemData +0x60`. It clears byte `+0x64` and
sets it when the extra-base result is positive. The initial literal at
`0x003458d0` is `0.0f`; a negative sum returns that zero. The new
`resolve_weapon_final_add_base_attack()` contract reproduces these gates,
sum, flag, and floor while taking the three helper results and source fields
as explicit inputs. The associated `getStarLevel()` helper returns the count
of the `CCArray` at `WeaponsItemData +0x98` minus one, or zero when empty.
`getAddHoleBase_atk()` returns zero for star levels at or below one; otherwise
it computes `(star_level + 2) * field_7c * (star_level - 1) * 0.5`, where
`field_7c` is at `WeaponsItemData +0x7c`. Its initial literal at `0x00345094`
is zero. `getAddTemperBase_atk()` doubles the signed integer at `+0xac` with
32-bit shift semantics, converts it to float, and multiplies by the same
`+0x7c` field. These helper contracts now provide the hole and temper inputs
to the final-add composition. The remaining `getExtraBase_atk()` is a complete
but unresolved 176-operation function; its returned value remains an input.
The native `getAttack()` has no visible empty-weapon fallback after
`equipAtIdx()`.

Gold subsystem and function-role labels for the seven functions are recorded in
`player-attack-value-labels.jsonl`. The local dataset v20 contains 9,158
labelled functions, 6,866 structural groups, and 9,739 gold label rows after
adding six new labels to manual-restoration v19. The generated corpus and
dataset remain outside Git.
