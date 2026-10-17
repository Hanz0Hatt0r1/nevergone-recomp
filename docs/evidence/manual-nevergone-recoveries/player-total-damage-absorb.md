# Manual reconstruction: total player damage absorption

> Address convention: executable addresses from RevEngInstr use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Function evidence

| Function | RevEngInstr entry | ELF entry | P-code ops | Emitted | Calls | Branches | Loads | Stores |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::getAllDamageAbsorb` | `0x0032c560` | `0x0031c560` | 265 | 256 | 20 | 15 | 27 | 0 |

The ML corpus row is truncated at its 256-op serialization limit. The full
Ghidra decompilation for the same shipped library contains the complete
function and supplies the arithmetic and object offsets below.

## Recovered calculation

`MEPlayer::getAllDamageAbsorb()` builds the base sum in this order:

1. `PlayerStatusInfo::getLevelDamageAbsorb()`.
2. The last `0x1c`-typed effect found in the equipment array at
   `MEEquip::equipAtIdx(..., 0) +0x98`; the native call also receives `1` and
   `ArmbandItemData +0x58`.
3. If the armband is equipped, `ArmbandItemData::getAbsorbAddition()` divided
   by `100.0f`.
4. `PlayerStatusInfo +0x140`.
5. `MEPlayer +0x118`, written by `setEquipAddDamageAbosr()`.
6. Enabled type `2` and `4` mission attributes, each adding
   `float(level)` multiplied by the value at `LevelUpData[type] +0x18`, then
   divided by `100.0f`.

The multiplier bonus comes from one of two paths. When
`AppParameters +0x214` is `4` or `8`, sum each type `10` management effect's
value at `LevelUpData[10] +0x1c` plus `float(level - 1)` multiplied by the
value at `LevelUpData[10] +0x20`. Otherwise, use `AppParameters +0x308`
directly. If equipped, add the armband multiplier at `ArmbandItemData +0x40`.
The function returns:

```text
base_sum * (multiplier_bonus + 1.0f + armband_multiplier)
```

`player_combat_stat_contract::resolve_damage_absorb()` mirrors the observed
iteration rules, effect gates, left-to-right additions, and final multiply.
Data-manager lookups and native effect getters remain explicit input terms so
the contract does not invent their missing producers.

This recovers the total damage-absorption calculation, not the separate
`MEPlayer::getMaxHealth()` function. Function-role and subsystem gold labels
are recorded in `player-total-damage-absorb-labels.jsonl`.
