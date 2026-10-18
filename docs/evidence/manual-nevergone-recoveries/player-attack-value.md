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
| `WeaponsItemData::getFinalAddMeat_atk` | `0x003453f8` | 59 | 4 | 5 | 3 | 2 |
| `WeaponsItemData::getFinalAddSupe_atk` | `0x00345538` | 59 | 4 | 5 | 3 | 2 |
| `WeaponsItemData::getFinalAddSupr_atk` | `0x00345678` | 99 | 5 | 10 | 8 | 6 |
| `WeaponsItemData::getAddTemperMeat_atk` | `0x00344eb0` | 24 | 0 | 2 | 3 | 0 |
| `WeaponsItemData::getAddTemperSupe_atk` | `0x00344edc` | 15 | 0 | 0 | 2 | 0 |
| `WeaponsItemData::getAddTemperSupr_atk` | `0x00344ef8` | 24 | 0 | 2 | 3 | 0 |
| `WeaponsItemData::getAddHoleMeat_atk` | `0x00344f88` | 29 | 1 | 1 | 1 | 0 |
| `WeaponsItemData::getAddHoleSupe_atk` | `0x00344fcc` | 29 | 1 | 1 | 1 | 0 |
| `WeaponsItemData::getAddHoleSupr_atk` | `0x00345010` | 29 | 1 | 1 | 1 | 0 |
| `WeaponsItemData::getAddMeat_atk` | `0x003449c0` | 136 | 13 | 13 | 22 | 0 |
| `WeaponsItemData::getAddSupe_atk` | `0x00344bb8` | 136 | 13 | 13 | 22 | 0 |
| `WeaponsItemData::getAddSupr_atk` | `0x00344cb4` | 136 | 13 | 13 | 22 | 0 |
| `WeaponsItemData::getExtraMeat_atk` | `0x00345338` | 109 | 9 | 7 | 13 | 0 |
| `WeaponsItemData::getExtraSupe_atk` | `0x00345478` | 109 | 9 | 7 | 13 | 0 |
| `WeaponsItemData::getExtraSupr_atk` | `0x003455b8` | 109 | 9 | 7 | 13 | 0 |
| `WeaponsItemData::getStarLevel` | `0x0034426c` | 15 | 2 | 1 | 2 | 0 |
| `WeaponsItemData::getAddHoleBase_atk` | `0x00345054` | 29 | 1 | 1 | 1 | 0 |
| `WeaponsItemData::getAddTemperBase_atk` | `0x00344f24` | 15 | 0 | 0 | 2 | 0 |
| `WeaponsItemData::getExtraBase_atk` | `0x00345734` | 176 | 14 | 13 | 20 | 0 |
| `WeaponsItemData::getAddBase_atk` | `0x00344db0` | 136 | 13 | 13 | 22 | 0 |
| `PlayerStatusInfo::add_C_AttackValue` | `0x00327644` | 14 | 1 | 1 | 2 | 1 |
| `PlayerStatusInfo::resetPlayerStatusClothAddData` | `0x003275b8` | 13 | 0 | 0 | 0 | 4 |
| `PlayerStatusInfo::add_C_HealthValue` | `0x003275cc` | 14 | 1 | 1 | 2 | 1 |
| `PlayerStatusInfo::add_C_StrengthValue` | `0x003275f4` | 14 | 1 | 1 | 2 | 1 |
| `PlayerStatusInfo::add_C_DamageAbsorbValue` | `0x0032761c` | 14 | 1 | 1 | 2 | 1 |
| `PlayerStatusInfo::add_EatProperty` | `0x003273f0` | 121 | 5 | 9 | 9 | 4 |
| `MRGetEatAttribute` | `0x003165a8` | 87 | 7 | 11 | 3 | 0 |
| `MRGetHeroEatAttribute` | `0x00316650` | 233 | 24 | 3 | 0 | 0 |

All thirty-two rows are complete. The original decompilation and ARM instructions
establish this order for `MEPlayer::getAttack()`:

1. `MEEquip::equipAtIdx(..., 0)` selects weapon-array entry zero from the
   `MEEquip` object held at `DataManager +0x24`; the array pointer is at
   `MEEquip +0x104`.
2. Load the weapon's base attack at `WeaponsItemData +0x38`.
3. Add `WeaponsItemData::getFinalAddBase_atk()`.
4. Add `PlayerStatusInfo::getLevelAttack()`, which reads `+0x110` from the
   `PlayerStatusInfo` object at `DataManager +0x48`. Its level-table producer is
   reconstructed in [player-level-attack-progression.md](player-level-attack-progression.md).
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

The sibling `add_C_HealthValue()`, `add_C_StrengthValue()`, and
`add_C_DamageAbsorbValue()` use the same `ManagementLayer +0x744 == 0` gate to
add their arguments at `+0x138`, `+0x13c`, and `+0x140`, respectively. Their
contracts update the corresponding status fields; `+0x138` feeds the final
additive term of `resolve_max_health()`, and `+0x140` is an input to
`resolve_damage_absorb()`.

`PlayerStatusInfo::add_EatProperty()` ignores additions while
`ManagementLayer +0x744` is nonzero. It accepts property ids `1`, `2`, `4`,
and `5`, checks the corresponding current `MRGetHeroEatAttribute()` result,
and adds to `+0x148`, `+0x154`, `+0x14c`, or `+0x150` only while that result is
below `2000.0f` for ids `1`/`2` or `200.0f` for ids `4`/`5`. The current level
is converted to a nonnegative integer before the helper call.

`MRGetHeroEatAttribute()` splits that integer into millions, thousands, and
units, looks up tiers `3`, `2`, and `1` in that order, multiplies each count by
the matching result, and adds the terms in order. `MRGetEatAttribute()` scans
CSV data rows in source order, matching column 9 to the tier and column 10 to
the type. The first matching row returns column 3 for type `1`, column 4 for
type `2`, column 5 for type `3`, or column 6 for type `4`; missing rows and
unsupported types return zero. Types `1`, `2`, and `4` look up their own CSV
types; type `5` looks up CSV type `3` and multiplies the sum by `0.01f`. The
one-million divisor, one-hundredth scale, lookup columns, and zero fallback are
confirmed in the shipped ELF.

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

`getFinalAddMeat_atk()` and `getFinalAddSupe_atk()` share a final composition:
start at `0.0f`; add the temper helper when `WeaponsItemData +0x18 == 0`; add the
hole helper when `ManagementLayer +0x744 != 0`; then add the extra helper and
the weapon field at `+0x40` (meat) or `+0x58` (supe). Each writes its extra
flag at `+0x44` or `+0x5c` according to whether the extra helper is positive,
and returns zero when the final sum is negative. The zero literals at
`0x00345474` and `0x003455b4` were confirmed in the shipped ELF.
`resolve_weapon_final_add_category_attack()` models this shared composition;
the category-specific helper outputs remain explicit inputs.

`getFinalAddSupr_atk()` also starts at zero and includes the hole helper under
the same management gate, then adds its extra helper. When
`WeaponsItemData +0x18 == 0`, it adds the temper helper and sets the special
`+0xe2` flag only if `WeaponsItemData +0xd4 == 0` and
`DataManager +0x54 +0x4c == 0`. When `+0x18 == 1`, it sets that flag if
`+0xd4 == 0`. It then adds `WeaponsItemData +0x48` and clamps negative sums to
zero; a set `+0xe2` flag overrides the result to zero and clears the extra flag
at `+0x4c`. The initial literal at `0x00345730` is `0.0f`.
`resolve_weapon_final_add_supr_attack()` reproduces this gate and flag
behavior, with helper results and manager fields supplied as decoded inputs.

The category-specific temper helpers use `WeaponsItemData +0xac` and these
scales: Meat `+0x6c`, Supe `+0x78`, and Supr `+0x70`. Meat returns zero when
byte `+0xe1` is set; Supr returns zero when byte `+0xe2` is set; Supe has no
additional byte gate. When enabled, each doubles the signed `+0xac` value with
32-bit shift semantics, converts it to float, and multiplies by the category
scale. The category hole helpers call `getStarLevel()` and use that same
category scale in `(star_level + 2) * scale * (star_level - 1) * 0.5`, returning
zero for star levels at or below one.

`getAddMeat_atk()`, `getAddSupe_atk()`, and `getAddSupr_atk()` scan the weapon
effect array at `WeaponsItemData +0x98` in order and start at zero. Their direct
effect types are Meat `0x1f`, Supe `0x21`, and Supr `0x22`. A matching direct
effect uses its special virtual getter only when `+0x18 == 0` and its `+0xa0`
name matches the equipped weapon; other direct effects use the regular getter.
Type `1000` scans its nested array at `+0x48`: matching child values use
`DataManager +0x54 +0x50 + 1.0f` as a multiplier when `+0x18 == 0`, the
alternate no-argument getter when `+0x18 == 1`, and contribute nothing for
other field values. `resolve_weapon_add_category_attack()` preserves both
array orders. Virtual getter values and the weapon-name comparison remain
decoded inputs.

`getExtraMeat_atk()`, `getExtraSupe_atk()`, and `getExtraSupr_atk()` start with
their matching `getAdd*` result plus `0.0f` and return it unchanged unless
`WeaponsItemData +0x18 == 0`. In AppParameters modes `4` and `8`, management
effect types Meat `4`, Supe `7`, and Supr `5` add the matching level-up base
bonus at `+0x1c` and `float(level - 1) *` its growth bonus at `+0x20`, skipping
level zero. Other modes use AppParameters offsets Meat `+0x2fc`, Supe `+0x2f0`,
and Supr `+0x2f8`. The helper then adds `DataManager +0x54 +0x38` and the
category EquipPropertyManager value at `+0x3c`, `+0x40`, or `+0x38`,
respectively. `resolve_weapon_extra_category_attack()` implements this
composition; decoded manager values and the initial `getAdd*` results remain
inputs.

Gold subsystem and function-role labels for these thirty-two functions are
recorded in `player-attack-value-labels.jsonl`. The local
manual-restoration-v62 dataset adds labels for twelve category helper
functions and contains 9,233 labelled functions, 9,881 label rows, and 6,939
structural groups; its split is 7,415 train / 996 validation / 822 test. The
generated corpus and dataset remain outside Git.
