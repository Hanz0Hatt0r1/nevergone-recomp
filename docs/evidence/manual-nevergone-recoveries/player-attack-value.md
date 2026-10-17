# Manual reconstruction: player attack value

> Address convention: executable addresses from RevEngInstr use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

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
| `WeaponsItemData::getExtraBase_atk` | `0x00345734` | 176 | 14 | 13 | 20 | 0 |
| `WeaponsItemData::getAddBase_atk` | `0x00344db0` | 136 | 13 | 13 | 22 | 0 |
| `PlayerStatusInfo::add_C_AttackValue` | `0x00327644` | 14 | 1 | 1 | 2 | 1 |
| `PlayerStatusInfo::resetPlayerStatusClothAddData` | `0x003275b8` | 13 | 0 | 0 | 0 | 4 |

All eleven rows are complete. The original decompilation and ARM instructions
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
`PlayerStatusInfo +0x144` is added last; its producer and reset path are now
recovered below.

`PlayerStatusInfo::add_C_AttackValue()` adds its float argument to `+0x144`
only while the byte at `ManagementLayer +0x744` is zero. A nonzero byte leaves
the accumulated value unchanged. `resetPlayerStatusClothAddData()` clears
`+0x138`, `+0x13c`, `+0x140`, and `+0x144`. The status-state contract mirrors
these writes; callers can supply
`PlayerStatusInfoState::cloth_add_attack_144` as
`AttackTerms::player_status_value_144` after applying the native gate and
reset behavior.

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
to the final-add composition. The complete `getExtraBase_atk()` helper is
reconstructed by `resolve_extra_base_attack()`: it always starts with the
value returned by `getAddBase_atk()` and applies extra bonuses only when
`WeaponsItemData +0x18 == 0`. In modes 4 and 8, each nonzero type-3 management
effect adds `LevelUpData[3] +0x1c` and `float(level - 1) * LevelUpData[3]
+0x20`; other modes use `AppParameters +0x2ec`. Enabled type 5, 6, and 10
attributes add `float(level) * LevelUpData[type] +0x18`. An equipped armband
adds `getAttackAddtion(ArmbandItemData +0x18)` and its `+0x38` field; the
function then adds `EquipPropertyManager +0x30`. The zero literal at
`DAT_0034584c` is confirmed in the shipped ELF. Manager reads, the
`getAddBase_atk()` result and armband getter output remain explicit inputs;
the former is reconstructed by the helper below. The native `getAttack()` has
no visible empty-weapon fallback after `equipAtIdx()`.

`getAddBase_atk()` now has a separate `resolve_weapon_add_base_attack()`
contract. It scans the `WeaponsItemData +0x98` array from start to end, starting
at the confirmed `0.0f` literal. A direct type-`0x1e` entry uses its special
virtual result when `WeaponsItemData +0x18 == 0` and the entry's `+0xa0`
string matches the equipped weapon's `+0xa0` name; otherwise it uses the
regular virtual result. A type-`1000` entry scans its nested array at `+0x48`
for type-`0x1e` entries. For weapon field `+0x18 == 0`, each nested result is
multiplied by `ArmbandItemData +0x50 + 1.0f`; for field value `1`, the alternate
no-argument getter result is added directly. Other field values add nothing
from nested entries. Virtual getter results and string comparisons are
decoded inputs, and the contract preserves the top-level and nested order of
float additions.

Gold subsystem and function-role labels for these eleven functions are
recorded in `player-attack-value-labels.jsonl`. Adding the four labels for
`add_C_AttackValue()` and `resetPlayerStatusClothAddData()` to manual-restoration
v37 and rebuilding as v38 produced 9,188 labelled functions, 6,896 structural
groups, 9,800 gold label rows, and splits of 7,379 train / 992 validation /
817 test. The generated corpus and dataset remain outside Git.
