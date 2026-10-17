# Online enemy action index and state-driven clips — 2026-10-17

Target: Never Gone 1.0.9 ARMv7
`libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.
Exports were produced read-only from the existing Ghidra project with the
generic RevEngInstr High-P-code and instruction exporters. Ghidra's image
addresses include the library's `0x10000` load base; this makes its entry
`0x002c0584` correspond to ELF offset `0x002b0584` used by older notes.

## State is the action-array index

The symbol-resolved `EnemyObject::onlineEnemyState(ENEMYSTATE, bool)` starts at
Ghidra image address `0x002c0584`. It stores the requested state in
`EnemyObject+0x274` at `0x002c0642`. Later, at `0x002c0726`, it calls
`cocos2d::CCArray::objectAtIndex` on the array at `EnemyObject+0x104`.

The RevEngInstr P-code row for that call has these arguments:

```text
CALL cocos2d::CCArray::objectAtIndex
  array = value loaded from EnemyObject+0x104
  index = value loaded from EnemyObject+0x274
```

At `0x002c071c`, native sets `r7 = 1`, but the next instructions store that
value at `EnemyObject+0x234`; the call index remains the state loaded from
`+0x274`. Thus state `0x1c` selects array index 28. The earlier interpretation
that `r7` was the lookup index was incorrect.

## Enemy03 action resources

`assets/enemy/enemy03/enemy_actions_1_03.plist` in the original OBB maps the
supported states as follows:

| State / action index | Action data | Loop | Primary frames | Atlas |
| --- | --- | --- | ---: | --- |
| 1 | `e03_e_w01.actData` | yes | 12 | `e03_default.plist` |
| 2 | `e03_e_r01.actData` | yes | 8 | `e03_default.plist` |
| 3 | `e03_e_a01.actData` | no | 13 | `e03_default.plist` |
| 4 | `e03_e_a02.actData` | no | 12 | `e03_default.plist` |
| 21 | `e03_e_i01.actData` | yes | 8 | `e03_default.plist` |
| 27 | `e03_e_s08.actData` | no | 17 | `e03_default.plist` |
| 28 | `e03_e_w01_back.actData` | yes | 12 | `e03_default.plist` |
| 43 | `e03_e_w02.actData` | yes | 12 | `e03_default.plist` |
| start 47 | `e03_e_born02b.actData` | no | 11 | `e03_02_default.plist` |

All nine selected `.actData` files passed the project-owned Sections A–G
parser when read from the OBB. The files remain outside the source repository.

The action-state selection now supports these eight looping/one-shot state
clips and the existing appearance clip. A materialized actor binding records
its immutable initial state plus its live state and the fixed game tick at
which a state change was observed. The renderer loads/caches the action document
by enemy/state and starts a changed state clip at elapsed time zero. It
preserves the appearance-to-state transition when the initial state remains
active.

## Movement boundary

RevEngInstr resolves `EnemyObject::actionTargetPos(ENEMYSTATE, MEPlayer*,
CCPoint, bool, bool)` to `0x002c188c` in the same Ghidra image. The exported body
reads/writes target-position fields at `EnemyObject+0x120/+0x124`; state values
`0x2b` and `0x1b` have explicit branches that bypass the ordinary target update.
This function is not yet reconstructed into the scene movement model. The new
state-driven clips only react to existing state updates; they do not invent AI
or generate walk/attack transitions.

One-shot state clips currently stop contributing frames after their parsed
duration. Native terminal-frame presentation is not established here and stays
open for a follow-up trace.
