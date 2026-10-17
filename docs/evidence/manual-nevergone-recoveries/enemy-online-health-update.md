# Manual reconstruction: online enemy health updates

> Address convention: executable addresses from RevEngInstr use an image base
> `0x1000` above the shipped ELF VMA. Subtract `0x1000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Function evidence

| Function | RevEngInstr entry | ELF entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `EnemyObject::setOlEnemyHealth` | `0x002c0870` | `0x002b0870` | 88 | 2 | 6 | 3 | 3 |

The RevEngInstr body is complete. The ELF symbol and ARM Thumb disassembly
confirm the entry at `0x002b0870`; the routine receives update value in `r1`
and update type in `r2`.

## Recovered update contract

- Type `1` converts the signed integer value to float and subtracts it from
  current health at `+0x164`. If the damage reaches or exceeds current health,
  the physical-animation damage amount is capped to the integer-converted
  current health; otherwise it uses the supplied damage.
- Type `2` converts the value to absolute health and writes `+0x164`. A
  decrease sends the integer-converted difference to the physical animation;
  an increase sends zero.
- Other types leave health unchanged and send zero physical damage.
- After any type, a current health value that is not positive is clamped to
  zero and requests `onlineEnemyState(0x15, true)`. Since that call is made
  with non-positive health and its force parameter set, the recovered state
  field becomes `0x15` when it differs. That transition clears the modeled
  no-death flag; if the enemy is already in `0x15`, the native handoff returns
  early and leaves the flag unchanged. Other handoff side effects remain open.
- Every path calls `EnemyPhysical::updatePhysicalState()` with the computed
  damage amount. The generation-checked roster now feeds this amount into its
  configured physical-state sequence and reports whether a state advanced.

The generation-checked tutorial roster and live attack gate now expose this
remote-update operation. The health and state fields reproduce the recovered
contract; callers receive the physical damage value and whether the death
state was requested.

`onlineEnemyState()` has 350 P-code operations and its corpus row is truncated
to 256, so this reconstruction does not claim to restore its complete action,
sprite, sound, and physics side effects. Gold labels are in
`enemy-online-health-update-labels.jsonl`. Local RevEngInstr dataset v31 has
9,179 labelled functions, 6,887 structural groups, and 9,782 gold label rows
(train 7,374, test 813, validation 992); generated corpus and splits remain
outside Git.
