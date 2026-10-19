# MEPlayer A attack from locomotion — 2026-10-18

## Target and evidence limits

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

`MEPlayer::setFunctionState(7)` requests `setActionState(0x02, false)` as
recorded in `docs/evidence/meplayer-set-function-state-base-gates-2026-10-12.md`.
The relevant `setActionState` entry is ELF `0x0031cf58`; the symbol inventory
records its Thumb entry as `0x0031cf59`. `MEPlayer::actionState()` begins at
ELF `0x0031e43c`.

REA accepted the original library, but Ghidra's initial whole-program
auto-analysis exceeded its 330-second deadline before returning procedure
evidence. The bounded branch observations below were cross-checked by direct
Thumb instruction decoding of the same hash-verified file. No original bytes or
disassembly listings are committed.

## Recovered state selection

`setActionState` reads the current MEPLAYERSTATE from `MEPlayer +0x278` and
uses it when resolving the A request:

- Current walk state `0x00`, requested state `0x02`: the branch at
  `0x0031d2c0..0x0031d2c8` selects state `0x10`.
- Current run state `0x01`, requested state `0x02`: the branch at
  `0x0031d2ce..0x0031d2d8` selects state `0x14`.
- Current initial state `0x1b` (and adjacent state `0x1c`): the path at
  `0x0031d2b0..0x0031d2c8` temporarily stores `0x27`; the later branch at
  `0x0031d7ca..0x0031d7d8` normalizes that value to `0x02` before the action
  array lookup at `0x0031d860..0x0031d870`.

`MEPlayer::setPlayerStateWithIdx` uses the state as the zero-based action-array
index, as recorded in `docs/tutorial-jump-player-action.md`. These requests
therefore resolve to `action17`, `action21`, and `action03`, respectively.

The full `MEPlayer::actionState()` switch has no transition branch for `0x10`
or `0x14`; after either action completes it returns that current state. Later
movement/control samples may choose walk, run, or idle through the separate
movement path. The reconstruction retains the completed state until that path
selects another action, and only permits a movement request after the action
document reports its recovered done flag.

## Clean-room implementation

`player_function_action_contract` now selects the A action from the committed
live MEPlayer state. Unknown states fail closed. The live action state accepts
only matching pairs (`walk -> 0x10`, `run -> 0x14`, `initial/0x1c -> 0x02`),
records completed movement attacks without inventing an automatic transition,
and allows subsequent movement to load the next action document.

No proprietary action payload or resource bytes are included.

## Local reconstruction dataset

The persistent RevEngInstr seed-label set adds three gold labels for
`source_entry=0032cf58` (`MEPlayer::setActionState`): game domain, combat
subsystem, and role `combat_player_locomotion_dependent_attack_state_selector`.
Rebuilding as `dataset-manual-restoration-v43` yields 9,195 labelled functions,
9,815 label rows, and 6,903 structural groups (train 7,385, validation 992,
test 818). The corpus and generated dataset remain in local user data; only the
evidence-backed label metadata is persisted there.
