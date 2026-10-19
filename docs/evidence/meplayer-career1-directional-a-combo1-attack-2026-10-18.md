# Career 1 directional A combo 1 attack — 2026-10-18

Target: Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so`, SHA-256
`94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`.

`Controls::tick()` emits function state `0x16` when A is pressed while
direction-combo state `1` is inside its 0.3-second window
([Controls tick evidence](controls-function-state-tick-2026-10-12.md)).

## Native action route

At ELF `0x00321098`, the function-state `0x16` branch starts at `0x003212f8`.
For Career 1 (`MEPlayer +0x144 == 1`) from the reconstructed initial action
state `0x1b`, it obtains the current direction from `Controls +0x120`, calls
`setPlayerDir()`, and calls `setActionState(0x0a, false)` at
`0x00321348`/`0x00321382`. Other current action states and Careers remain
outside this reconstructed route.

## Action data and completion

The selected Career 1 `p01_actions.plist` from the imported original data has
SHA-256
`f0cfba7dba54a17c055d1894823a103956220f0f9aaf4c71c25833aa5b96c514`.
Its `action11` record maps MEPLAYERSTATE `0x0a` to `p01_s01.actData` with
`looping=false`. The local plist is user-owned imported data; neither it nor the
action-data bundle is committed. The existing
[Career 1 actionsData evidence](player-actionsdata-container-2026-10-12.md)
records all 53 plist action names as present in the 125-entry bundle, so the
runtime can resolve this action by basename.

`MEPlayer::actionState()` at ELF `0x0031e43c` has no completion transition for
state `0x0a`; its default return preserves the current action state. On
Controls state `0`, `MEPlayer::setFunctionState()` first waits for the current
action to be done, then the default idle path calls
`setActionState(0x1b, false)`. The live model therefore records the completed
`0x0a` in place and queues `0x1b` only when both the Controls state is zero and
the parsed non-looping action is done. It also reevaluates the zero state on
subsequent ticks, matching the native per-tick call.

Gold labels for the recovered `setFunctionState` dispatch and `actionState`
completion roles are recorded in
[`player-directional-a-combo1-attack-labels.jsonl`](manual-nevergone-recoveries/player-directional-a-combo1-attack-labels.jsonl).
