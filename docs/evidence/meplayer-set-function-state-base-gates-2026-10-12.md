# MEPlayer::setFunctionState base gates — 2026-10-12

## Clean-room boundary

This note records bounded static behavior recovered from the shipped ARMv7 `libcocos2dcpp.so`. No original binary, disassembly dump, screenshot, or proprietary asset is committed.

The shipped symbol is `MEPlayer::setFunctionState(CONTROLSSTATE)` at ELF `0x00321098` (Ghidra image-base view `0x00331098`). `GameScene::setFunctionState` forwards the live Controls function state to this method once per scene tick.

## Head gates

Before the base A/B/C dispatch, the original reads current MEPlayer action state at `+0x278` and rejects immediately when it is:

- `0x42`;
- any state in `0x39..0x3c`;
- `0x3f`.

There is also a tutorial-specific head lock. When:

- `AppParameters +0x214 == 0x0b`;
- `GameSceneUI::lessonStep() == 0x0c`;
- MEPlayer float `+0x328 >= 100.0f`;

only function state `0x1c` is allowed through. Every base A/B/C request instead writes zero to MEPlayer `+0x1f4` and returns. The `100.0f` threshold is the literal `0x42c80000` referenced by the function.

## Repeat handling

MEPlayer `+0x1f4` is the retained function state.

- If the requested state equals the retained state, the original normally returns without dispatch.
- Function state `8` (B) is the exception: it may re-evaluate even when already retained.
- B is rejected while current action state is `0x24`, whether the previous function state already equals 8 or not.
- Once these gates pass, the requested function state is written to `+0x1f4` before the branch-specific action logic.

## Base A and C

The already recovered base mappings are direct after the common gates:

| Function state | Control | setActionState |
| ---: | --- | --- |
| `7` | A | `(0x02, false)` |
| `9` | C / jump | `(0x20, false)` |

The A branch contains tutorial pause side effects when no CP progress exists and the lesson is at step 8, but its base action remains `0x02,false`.

## B is a three-outcome branch

Function state `8` is not merely a guarded `setActionState(0x11,true)` call.

Define the special B condition as either:

- MEPlayer `+0x1f0` equals `0x12` or `0x13`; or
- MEPlayer `+0x1f8` equals `0x11` or `0x0f`.

Then:

1. if the special condition is true and current action state is `0..9`, dispatch `(0x11, true)`;
2. if the special condition is true and current action state is greater than 9, dispatch fallback `(0x1d, false)`;
3. if the special condition is false and current action state is exactly `0x0c`, keep function state 8 latched but make no `setActionState` call;
4. otherwise dispatch fallback `(0x1d, false)`.

This replaces the earlier coarse project description that B simply had an unspecified additional guard.

## Project contract

`meplayer_function_gate_contract::evaluate_base()` now models only the common head/repeat gates and the base function states 7/8/9. Synthesized states such as `0x14..0x1c` deliberately return unsupported until their individual branches are represented.

The contract remains pure and does not execute player animation. A later live executor must provide current action state plus the recovered MEPlayer fields before applying its result.
