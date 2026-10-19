# Manual reconstruction: enemy online state transitions

> Address convention: executable addresses from RevEngInstr use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.
> Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Function evidence

| Function | RevEngInstr entry | ELF entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: |
| `EnemyObject::onlineEnemyState` | `0x002c0584` | `0x002b0584` | 350 | 26 | 63 | 35 | 17 |

The corpus row emits 256 of 350 P-code operations. The saved Ghidra decompilation
contains the complete body. The request gates and state writes below are
implemented; downstream action, sprite, sound, bounds, and physics calls are
still open.

## Recovered transition contract

The new `enemy_online_state_contract` mirrors these observed steps in order:

1. Ignore state `0x2f`, then return unchanged when the requested state equals
   the current state.
2. Unless the caller forces the transition, reject it when the current attack
   frame is past the `isAttackMiss()` cutoff.
3. Apply the health gate only when health is positive or the current state is
   `0x15`/`0x16`. In current states `4..9`, `0x0f`, `0x10`, `0x18`, or `0x2c`,
   beaten-state requests can bypass a running action. The exceptional current
   state `0x18` remaps requests `5`, `6`, `0x0f`, and `0x2c` to `0x19`.
4. In current state `0x1a`, reject beaten-state requests under that same health
   gate.
5. After those checks, a queued state at `+0x278` overrides the request and is
   reset to sentinel `0x2f`; the resolved state is then written to `+0x274`.
6. Accepting state `0x15` or `0x16` also signals the caller to clear the global
   enemy-state flag. The online-health result now carries that request; the
   project still has no modeled `AppParameters +0x1f8` owner to apply the byte
   write itself.

The recovered death update calls `onlineEnemyState(0x15, true)` after health is
clamped to zero. That path now uses this transition contract, so the unchanged
state `0x15` early return preserves the no-death byte exactly as the native
handoff does.

This reconstruction does not claim the complete routine. In particular it
does not execute the native action-array selection, animation handoff, action
sound, hit-point/bounds refresh, `PhysicsSmash`, or scene-gate callback. The
function-role and subsystem gold labels are in
`enemy-online-state-transition-labels.jsonl`.

The local manual-restoration v33 dataset contains 9,182 labelled functions,
6,890 structural groups, and 9,788 gold rows (train 7,375, test 815,
validation 992). The generated corpus and splits remain outside Git.
