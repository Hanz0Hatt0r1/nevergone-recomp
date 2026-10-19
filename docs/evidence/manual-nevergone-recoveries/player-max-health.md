# Manual reconstruction: maximum player health

> Address convention: executable addresses from RevEngInstr use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Function evidence

| Function | RevEngInstr entry | ELF entry | P-code ops | Emitted | Calls | Branches | Loads | Stores |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::getMaxHealth(unsigned int)` | `0x0032bfa4` | `0x0031bfa4` | 324 | 256 | 18 | 20 | 27 | 1 |

The RevEngInstr serialization is truncated at its 256-op limit. The complete
Ghidra decompilation for the same binary was used for the reconstruction.

## Recovered calculation

The level-management bonus uses type `9` effects when `AppParameters +0x214`
is `4` or `8`: for each nonzero level, add `LevelUpData[9] +0x1c` and
`float(level - 1) * LevelUpData[9] +0x20`. Other modes use
`AppParameters +0x304`. Enabled type `7` and `8` attribute effects add
`float(level) * LevelUpData[type] +0x18` without a percentage division.

The armband contributes its `getBloodAddition(...)` result and multiplier at
`ArmbandItemData +0x3c` only when equipped. If the mercenary map exists and
`LansquenetManager::isAllDead()` is false, the native loop uses the health
value of the first entry whose type is `6`. This contract accepts that decoded
value as an input, keeping the native container traversal outside the formula.
The weapon contributes `WeaponsItemData::getAddHelath()` (native spelling),
the player contributes `MEPlayer +0x120`, and level health is offset by the
literal `100.0f`.

The function sums those values in native order, multiplies by
`1.0f + armband_multiplier`, then adds `MEPlayer +0x110` and
`PlayerStatusInfo +0x138`. It converts the result to a signed integer, stores
that integer at `MEPlayer +0x11c`, and returns the cached integer converted to
float. `resolve_max_health()` mirrors the order, gates, truncation, cache
write, and return conversion; native manager lookups and getters remain input
terms.

Subsystem and function-role gold labels are in
`player-max-health-labels.jsonl`. Adding them to manual-restoration v34 and
rebuilding as v35 produced 9,184 labelled functions, 6,892 structural groups,
9,792 gold label rows, and splits of 7,376 train / 992 validation / 816 test.
The generated corpus and dataset remain outside Git.
