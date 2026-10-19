# Manual reconstruction: player attack-state gate

> RevEngInstr executable addresses use an image base `0x10000` above the
> shipped ELF VMA. Gold-label `source_entry` retains the executable address;
> subtract `0x10000` to inspect the ELF.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::isAttackState` | `0x0032c76c` | 31 | 0 | 9 | 1 | 0 |

The corpus row is complete. Thumb disassembly at ELF VMA `0x31c76c` loads
`MEPlayer +0x278` and compares the current action state against unsigned ranges
and exact exclusions. `player_attack_state_contract::is_attack_state()` keeps
the native 32-bit wrapping subtractions and returns true except when the full
exclusion conjunction holds, with state `0x3f` explicitly accepted.

`BattleManager::playerHitEnemy()` calls this gate before requesting
`MEPlayer::curActionHitPointCount()` and iterating `MEPlayer::beatHitPoint()`;
its call site is ELF VMA `0x2a835a`. The existing player render queue now marks
each Section-E hitpoint candidate with the same eligibility result, but only
when the loaded live action is bound to the current spawn and matches its
requested action state. This preserves all rendered hitpoint records while
making the native attack gate available to a later collision consumer.

This does not yet perform the player-point/enemy-bounds comparison or apply
enemy damage. The transformed player hitpoint centers are reconstructed, but
the native melee loop and its hit, combo, damage, and reaction side effects are
still not connected to the direct-render runtime.

Subsystem and function-role gold labels are recorded in
`player-attack-state-gate-labels.jsonl`. Adding the two rows to manual
restoration v52 and rebuilding as v53 adds one function and two gold rows:
9,209 labelled functions, 6,916 structural groups, and 9,832 gold rows
(7,397 train, 819 test, 993 validation). Generated dataset files remain
outside Git.
