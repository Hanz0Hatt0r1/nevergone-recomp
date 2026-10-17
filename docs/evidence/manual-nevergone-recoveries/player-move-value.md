# Manual reconstruction: player movement delta

> Address convention: RevEngInstr executable addresses use an image base
> `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect the ELF.
> Gold-label `source_entry` values intentionally keep the corpus address.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## Selection

| Function | RevEngInstr entry | ELF VMA | P-code ops | Branches | Calls |
| --- | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::playerMoveValue` | `0x0032d9c0` | `0x0031d9c0` | 648 (256 emitted) | 53 | 12 |

The RevEngInstr corpus row is truncated at its 256-op model limit. The shipped
Thumb body and local Ghidra decompilation were used to recover the bounded
paths below; the function's full state machine is not claimed as reconstructed.

## Direct input paths

The input is normalized Controls direction, not joystick power. With the
player's `+0x150` byte clear, MEPLAYERSTATE `0` uses `(x * 8, y * 4)` for player
indices 1 through 3. State `1` uses `(x * 20, y * 10)` for player 1 and
`(x * 16, y * 8)` for player indices 2 and 3. The initial idle state `0x1b`
leaves the function's zero-initialized result unchanged.

Base A state `0x02` and movement attacks `0x10/0x14` consume the X component of
`MeActionsSystem::curMoveValue()` and clear its accumulator. This project now
reconstructs that value from successive rendered action-frame bounds. B states
`0x1d/0x1e` return zero. Jump states `0x20/0x21` consult `MEPlayer +0x3c5`; the
current C/jump route starts from idle, so the recovered flag is clear and state
`0x20` returns zero. Other jump-entry and knockback branches remain unsupported.

## Runtime connection

The joystick still uses its requested movement mode to select walk/run action
documents. The fixed-tick motion cursor now receives the committed live
MEPLAYERSTATE instead. It therefore stays still while initial idle is loaded or
while an unsupported action state is active. It applies direct input movement in
states `0/1` and the one-shot action-frame displacement in states `0x02/0x10/0x14`.

The implementation assumes the normal player path where `MEPlayer +0x150` is
clear. The frame-driven accumulator is described in
`player-action-frame-move-value.md`; knockback, alternate jump entry, and the
nonzero `+0x150` player path remain open.

The function-role gold label is recorded in
`player-move-value-labels.jsonl`. The corpus and generated dataset remain
outside Git. Adding this label and the no-collision setter label to manual
restoration v50 and rebuilding as v51 produces 9,206 labelled functions,
6,913 structural groups, and 9,828 gold rows (7,396 train / 818 test / 992
validation). Adding the two action-frame movement labels advances the local
dataset to v52 with 9,208 labelled functions, 6,915 structural groups, and
9,830 gold rows (7,397 train / 818 test / 993 validation).
