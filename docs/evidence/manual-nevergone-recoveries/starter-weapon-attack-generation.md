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

The raw asset files are not included in this repository. All six base attack
fields and their scales are identical in both files:

| `.wa` byte offset | `WeaponsItemData` field | Value |
| ---: | ---: | ---: |
| `0x1c` | `+0x38`, base attack | `18.0f` |
| `0x20` | `+0x40` | `15.0f` |
| `0x24` | `+0x48` | `16.0f` |
| `0x28` | `+0x50` | `23.0f` |
| `0x2c` | `+0x58` | `15.0f` |
| `0x30` | `+0x60`, base attack addition | `54.0f` |
| `0x38` | `+0x68` scale | `2.0f` |
| `0x3c` | `+0x6c` scale | `1.0f` |
| `0x40` | `+0x70` scale | `1.0f` |
| `0x44` | `+0x74` scale | `1.0f` |
| `0x48` | `+0x78` scale | `1.0f` |
| `0x4c` | `+0x7c` scale | `2.0f` |

## Native loader and generation path

`WeaponsItemData::LoadDataFile()` starts at ELF VMA `0x00334564`. Its
`HPData::getBytes()` calls read four-byte fields at the offsets above into the
listed object offsets. The loader reads the unsigned integer at file offset
`0x34`; when it is greater than 4, it sets `+0x84 = value - 5` and
`+0x88 = value + 5`. At values 4 or below it sets those fields to 0 and 10.
Both starter files contain 35, so their ranges become 30 and 40.

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
the same fraction across base fields `+0x38`, `+0x40`, `+0x48`, `+0x50`, and
`+0x58`; it retains the selected value at `+0x60`. For quality tiers 0 and 1,
the fields are equivalent to:

```text
k = min + fraction * (max - min)
field[i] = source_field[i] + source_scale[i] * k
for i = +0x38, +0x40, +0x48, +0x50, +0x58
+0x60 = source +0x60 + source +0x7c * k
```

For tier 2, `min == max == 10`, so all six fields receive exactly ten times
their respective scale. The builder also computes, before that boost,
`+0x8c = (+0x84 + +0x88) * 0.5`, `+0x90 = +0x38`, and
`+0x94 = (-4 * +0x38) / (+0x88 - +0x84)^2`. For both starter files this is
`+0x8c = 35`, `+0x90 = 18`, and `+0x94 = -0.72`. The tier ranges are
`[0, 5)`, `[5, 10)`, and `10`.

## Saved original-profile cross-check

The connected original installation was already running. Its private
`DMG_01.sData` was read locally and decoded with key `1`, which
`GameSaveData::getFileData()` at ELF VMA `0x0033a7aa` passes to `Decode()`.
`WeaponsItemData::SaveData()` at ELF VMA `0x00333b50` serializes the six base
fields in object-offset order, followed by the range and curve fields. The
decoded weapon record contained base fields
`[38, 25, 26, 33, 25, 74]`, range `[30, 40]`, curve fields `[35, 18, -0.72]`,
and quality tier `2`. These match applying `k = 10` to the `p01` starter `.wa`
values. The private save file itself, including its profile identifier, was
not added to the repository.

The helper also rolls property flags after changing the base stats. It starts
with `+0xc8`, `+0xcc`, `+0xd0`, `+0xd4`, `+0xd8`, and `+0xdc` set to one. Its
next normalized roll clears `+0xd4`, `+0xd8`, and `+0xcc` when the roll is at
most `0.1`; for rolls in `(0.1, 0.3]` it repeatedly selects two distinct
properties; for `(0.3, 0.8]` it selects one; above `0.8` it leaves the flags
unchanged. `elevateBaseProperty()` chooses among selectors `{3, 4, 1}` and
clears `+0xd4`, `+0xd8`, and `+0xcc`, respectively. The new
`resolve_native_weapon_property_build()` contract preserves the six flags,
selected property ids, and the number of selector rolls consumed, including
retries when the two-property branch selects the same property twice. A
normalized selector that reaches index 3 is rejected because the native code
would read beyond its three-entry selector table. This edge case is distinct
from the base-stat interpolation, where a rounded `1.0f` is a valid endpoint.
The existing `resolve_attack()` contract is not called by a live gameplay path
yet, so this recovery records the exact generation inputs and formulas without
claiming runtime combat is restored.

`player_combat_stat_contract::parse_native_weapon_attack_file()` reads all six
base fields, their scales, and the derived range without embedding the original
binary assets. `resolve_native_weapon_attack_build()` takes the two normalized
native rolls as explicit inputs (including `1.0f` if conversion rounding
reaches that endpoint), then returns the six generated fields and native
weapon-curve inputs. `resolve_native_weapon_property_build()` accepts the
following property roll and selector rolls separately. RNG seeding remains
outside these contracts. Gold subsystem and role labels for the loader,
builder, stat/property rolls, save serializer, and save-file decoder are in
`starter-weapon-attack-generation-labels.jsonl`.
