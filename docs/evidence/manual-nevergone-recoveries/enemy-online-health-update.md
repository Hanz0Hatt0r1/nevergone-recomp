# Manual reconstruction: online enemy health updates

> Address convention: executable addresses from RevEngInstr use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
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
  zero and requests `onlineEnemyState(0x15, true)`. The recovered transition
  gates now model this forced request: it bypasses attack-miss checks, applies
  no health gate from ordinary states at zero health, and leaves the no-death
  flag unchanged on the same-state early return. The update result reports the
  global enemy-state flag clear request for an accepted `0x15`/`0x16` state;
  the actual `AppParameters +0x1f8` byte write remains outside this model.
  Other handoff side effects remain open; see
  `enemy-online-state-transition.md`.
- Every path calls `EnemyPhysical::updatePhysicalState()` with the computed
  damage amount. The generation-checked roster now feeds this amount into its
  configured physical-state sequence and reports whether a state advanced.

The generation-checked tutorial roster and live attack gate now expose this
remote-update operation. The health and state fields reproduce the recovered
contract; callers receive the physical damage value and whether the death
state was requested.

`onlineEnemyState()` has 350 P-code operations and its corpus row is truncated
to 256; the complete state gates are reconstructed separately, while its
action, sprite, sound, and physics side effects remain open. Gold labels are in
`enemy-online-health-update-labels.jsonl`. Local RevEngInstr dataset v33 has
9,182 labelled functions, 6,890 structural groups, and 9,788 gold label rows
(train 7,375, test 815, validation 992); generated corpus and splits remain
outside Git.
