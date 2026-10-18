# Manual reconstruction: starter weapon attack generation

> Address convention: RevEngInstr executable addresses use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values keep the RevEngInstr address.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Starter data observed on the connected original install

The original app's private asset directory contains these 80-byte files:

| File | SHA-256 | word at file offset `0x0c` |
| --- | --- | ---: |
| `weapons/wa/wa_p01_001_001.wa` | `09a1832bcb5ce07e28a33daafe411ce30b68d275d1351d2e3ccb2952cfe4b83a` | 9 |
| `weapons/wa/wa_p02_001_001.wa` | `24f53964019824617ac4ed5422c138d4a0658e1fbc8bbc05559761075acdfb1a` | 1 |

The raw asset files are not included in this repository. The four attack inputs
are identical in both files:

| `.wa` byte offset | `WeaponsItemData` field | Value |
| ---: | ---: | ---: |
| `0x1c` | `+0x38`, base attack | `18.0f` |
| `0x30` | `+0x60`, base attack addition | `54.0f` |
| `0x38` | `+0x68`, base attack scale | `2.0f` |
| `0x4c` | `+0x7c`, base attack-addition scale | `2.0f` |

## Native loader and generation path

`WeaponsItemData::LoadDataFile()` starts at ELF VMA `0x00334564`. Its
`HPData::getBytes()` calls read four-byte fields at the offsets above into the
listed object offsets. The loader also reads the integer at file offset
`0x34`; for values over 9 it derives `+0x84 = value - 5` and
`+0x88 = value + 5`. In these two starter files that integer is 35, so those
fields become `30.0f` and `40.0f`.

`WeaponsItemData`'s constructor at ELF VMA `0x00333630` clears `+0xe4` and
`+0x410`, and clears `+0xe8`. `GameSaveData::initGameSaveData()` creates the
starter weapon and calls `buildFullWeaponsData()`; its `+0xe8 == 0` guard
allows the first build to proceed. The constructor's `+0x410 == 0` state
allows `elevateBaseRAttribute()` to apply the random roll.

`WeaponsItemData::buildFullWeaponsData()` at ELF VMA `0x00333a84` calls
`lrand48()` to select `+0xe4`, the quality tier:

| Normalized first roll | Tier | `elevateBaseRAttribute(min, max)` | Probability |
| --- | ---: | --- | ---: |
| `<= 0.05` | 2 | `(10, 10)` | 5% |
| `> 0.05` and `< 0.20` | 1 | `(5, 10)` | 15% |
| `>= 0.20` | 0 | `(0, 5)` | 80% |

`WeaponsItemData::elevateBaseRAttribute(int, int)` at ELF VMA `0x003338a8`
consumes the next random value to select the scaled `+0x60` value. It uses
that selected value to derive a shared interpolation fraction, then applies
the same fraction across the base attack fields. For quality tiers 0 and 1,
the attack results are equivalent to:

```text
k = min + fraction * (max - min)
+0x38 = source +0x38 + source +0x68 * k
+0x60 = source +0x60 + source +0x7c * k
```

For tier 2, `min == max == 10`, so both fields receive exactly ten times their
respective scale. On the two starter files this gives `+0x38 = 18 + 2k` and
`+0x60 = 54 + 2k`; their sum before temper, hole, equipment, level, and status
bonuses is `72 + 4k`. The tier ranges are `[0, 5)`, `[5, 10)`, and `10`.

The helper also rolls property flags after changing the base stats. It starts
with `+0xc8`, `+0xcc`, `+0xd0`, `+0xd4`, `+0xd8`, and `+0xdc` set to one. Its
next normalized roll clears `+0xd4`, `+0xd8`, and `+0xcc` when the roll is at
most `0.1`; for rolls in `(0.1, 0.3]` it repeatedly selects two distinct
properties; for `(0.3, 0.8]` it selects one; above `0.8` it leaves the flags
unchanged. `elevateBaseProperty()` chooses among selectors `{3, 4, 1}` and
clears `+0xd4`, `+0xd8`, and `+0xcc`, respectively. These flag changes are
outside the attack-value contract added here. The existing
`resolve_attack()` contract is not called by a live gameplay path yet, so this
recovery records the exact file inputs and attack-generation formula without
claiming runtime combat is restored.

`player_combat_stat_contract::parse_native_weapon_attack_file()` reads the
four little-endian fields without embedding the original binary assets.
`resolve_native_weapon_attack_build()` takes the two normalized native rolls
as explicit inputs (including `1.0f` if conversion rounding reaches that
endpoint) and reproduces the tier choice and resulting `+0x38` and `+0x60`
values. Keeping the rolls explicit makes the function suitable as a
deterministic training and reconstruction example while leaving RNG seeding
and property-flag generation as separate work.
