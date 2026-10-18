# Manual reconstruction: player combo number

> Address convention: RevEngInstr executable addresses include the recovered
> image base, `0x10000` above the shipped ELF VMA. Subtract `0x10000` to inspect
> the ELF. Object-field offsets are unchanged.

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

## RevEngInstr selection

| Function | Entry | P-code ops | Calls | Branches | Loads | Stores |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| `MEPlayer::addComboNum` | `0x00329ac2` | 11 | 0 | 0 | 1 | 2 |
| `MEPlayer::getPlayerComboNum` | `0x00329ad4` | 5 | 0 | 0 | 1 | 0 |

The exact Thumb bodies in the shipped library are 12 and 4 bytes,
respectively. `addComboNum()` loads the 32-bit value at `MEPlayer +0x14c`, adds
one, and stores it back. It then stores zero at `MEPlayer +0x2f0`. The adjacent
`getPlayerComboNum()` getter returns the value at `+0x14c`, confirming that
field as the combo number. The separate `+0x2f0` field is kept under its raw
offset because its meaning is not established here.

`player_combo_num_contract` reproduces those two operations with unsigned
32-bit arithmetic. This is a leaf-function reconstruction; it is not yet
connected to the live combat path. A geometric contact or an open hit window
alone does not count as a landed hit, so the runtime must wait for a verified
damage event before calling the increment operation.

Gold subsystem and function-role labels for both entries are recorded in
`player-combo-number-labels.jsonl`. Generated RevEngInstr corpora and datasets
remain outside Git.
