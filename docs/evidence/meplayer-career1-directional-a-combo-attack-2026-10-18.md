# Career 1 directional A combo attack — 2026-10-18

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

`Controls::tick()` emits function state `0x14` when A is pressed while
direction-combo state `10` is still inside its recovered 0.4-second window
([Controls tick evidence](controls-function-state-tick-2026-10-12.md)).
The reconstructed Controls path retains that function-state edge and the
committed MEPlayer action index.

## Native action route

At ELF `0x00321098`, `MEPlayer::setFunctionState(CONTROLSSTATE)` handles input
state `0x14` at `0x00321208`. It rejects the recovered special action-state
ranges, beaten states, action state `0x32`, and a current action state `0x0c`.
For the reconstructed initial state `0x1b`, the route:

1. obtains the direction from `Controls +0x120` and calls `setPlayerDir()`;
2. checks `MEPlayer +0x144`; when it is `1` (Career 1), calls
   `setActionState(0x0c, true)` at `0x003212f2`/`0x00321380`.

Career 1 is explicitly gated in the reconstruction, as are the current action
state `0x1b` and the pending function state/request pair. Other careers and
other current action states remain unsupported here.

## Completion chain

`MEPlayer::actionState()` at ELF `0x0031e43c` advances completed action state
`0x0c` to `0x0d`, `0x0d` to `0x0e`, and `0x0e` to `0x1b` when the career index
is `1`. The native routine has additional Career 2 (`+0x144 == 2`) branches for
these actions; those are not modeled by this reconstruction.

The live boundary loads each requested Career-1 action through the existing
player-action document loader and binds completion to the current generation
and loaded revision. This recovers the directional combo action sequence; it
does not claim damage application because the attack-stat producer remains
unresolved ([damage evidence boundary](manual-nevergone-recoveries/player-damage-core.md#runtime-data-boundary-checked-on-device)).

Gold labels for the recovered `setFunctionState` dispatch and `actionState`
completion roles are recorded in
[`player-directional-a-combo-attack-labels.jsonl`](manual-nevergone-recoveries/player-directional-a-combo-attack-labels.jsonl).
