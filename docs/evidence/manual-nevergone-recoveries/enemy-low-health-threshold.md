# Manual reconstruction: enemy low-health threshold

> Address convention: executable addresses from RevEngInstr use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Function identity and address mapping

| Function | RevEngInstr entry | ELF entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `EnemyObject::isLowHealthPercent` | `0x002c4f80` | `0x002b4f80` | 28 | 0 | 2 | 2 | 0 |

The dataset label keeps corpus entry `0x002c4f80`; the shipped ELF
exports `EnemyObject::isLowHealthPercent()` at `0x002b4f81` with the Thumb bit
set, so the instruction address is `0x002b4f80`.

## Recovered predicate

The function loads current health from `+0x164` and max health from `+0x160`,
computes `current / max * 100`, converts that percentage to a signed integer
with ARM VFP truncation, converts the integer back to float, and returns whether
it is below `30.0`. For finite percentages this is equivalent to
`health_ratio_percent(state) < 30.0f`.

The native function has no zero-max guard. The clean-room roster only creates
health state when imported base health is positive, so the modeled gameplay
path has a valid denominator. `health_ratio_percent()` retains its existing
defensive behavior for invalid state supplied outside that path.

## Runtime integration

`enemy_health_state::is_low_health_percent()` implements the predicate. The
tutorial roster snapshot counts live enemies below the threshold, and the
attack-wave status report exposes that count. Dead enemies are excluded from
this aggregate; the original predicate itself only examines health.

The function-role and combat-subsystem gold labels are in
`enemy-low-health-threshold-labels.jsonl`. The local RevEngInstr dataset v27
has 9,172 labelled functions, 6,880 structural groups, and 9,768 gold label
rows (train 7,367, test 813, validation 992). Its generated corpus and split
files remain outside Git.
