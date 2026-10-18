# Career 1 directional A attack — 2026-10-18

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

`Controls::tick()` emits function state `0x15` when A is pressed while
direction-combo state `2` is still inside its recovered 0.3-second window
([Controls tick evidence](controls-function-state-tick-2026-10-12.md)). The
existing live input path retains that exact state and the committed MEPlayer
action index.

## Native action route

At ELF `0x0032124a`, `MEPlayer::setFunctionState(0x15)` reads the current
MEPLAYERSTATE. States `0x20..0x21`, `0x19..0x1a`, and `0x35..0x36` take a
separate fallback path. From the reconstructed initial state `0x1b`, the
ordinary route:

1. obtains the current player direction from `Controls +0x120`;
2. calls `MEPlayer::setPlayerDir()`;
3. calls `MEPlayer::setActionState(0x0f, false)`.

`MEPlayer::setPlayerIdx()` at ELF `0x00324e94` stores the selected Career at
`MEPlayer +0x144`. On action completion, `MEPlayer::actionState()` at ELF
`0x0031e43c` returns state `0x0f` to `0x1b` when that player index is `1`.
Other careers use `MEPlayer +0x3d0/+0x3c8` through
`setPlayerStateWithIdx()`; this reconstruction does not model those inputs.

## Reconstructed boundary

The live request accepts only function state `0x15`, Career `1`, and current
state `0x1b`. It resolves the normal imported Career-1 `action16` data through
the existing player-action loader. The parsed action done flag requests the
native `0x1b` completion state, with the existing generation/revision checks.
All other Careers and current action states remain fail-closed.

Gold labels for the recovered `setFunctionState` dispatch and `actionState`
completion roles are recorded in
[`player-directional-a-attack-labels.jsonl`](manual-nevergone-recoveries/player-directional-a-attack-labels.jsonl).
