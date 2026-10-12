# SceneActions timed trigger schedule evidence — 2026-10-13

## Scope

This note records bounded control flow recovered from the shipped Never Gone 1.0.9 ARMv7 `GameScene::TriggerSceneAction(float)` routine. No original binary, disassembly listing, asset bytes, or decompiler output is committed.

The exact dynamic symbol is retained in the shipped library. Clearing the ARM Thumb bit places the routine at code address `0x00344934` in the verified original ELF (`SHA-256 94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`).

## Recovered retained state

Each invocation adds its float argument to one retained float at `GameScene +0x240` before checking any event threshold.

Eight adjacent bytes at `GameScene +0x244` through `+0x24b` are one-shot guards for eight event dispatches. Once a guard is set, that event is skipped by later invocations.

## Threshold and event order

The native routine checks these pairs in order:

| index | strict threshold | event id | one-shot byte |
| ---: | ---: | ---: | --- |
| 0 | `5.0f` | `2001` | `+0x244` |
| 1 | float32 `5.4f` | `2002` | `+0x245` |
| 2 | float32 `5.8f` | `2003` | `+0x246` |
| 3 | float32 `6.2f` | `2004` | `+0x247` |
| 4 | float32 `6.6f` | `2005` | `+0x248` |
| 5 | `7.0f` | `2006` | `+0x249` |
| 6 | float32 `7.4f` | `2007` | `+0x24a` |
| 7 | float32 `7.8f` | `2008` | `+0x24b` |

The comparison is strict. The branch skips dispatch while `accumulated <= threshold`, so equality does not fire an event.

After a newly eligible event is dispatched, its byte is set and the routine exits. Therefore even a very large accumulated value releases at most one new event per `TriggerSceneAction(float)` invocation. A later invocation, including one with zero delta, can release the next unfired event when the accumulator remains above its threshold.

## Layer dispatch boundary

For each fired event, the original iterates eleven retained `GameSceneClassLayer*` slots beginning at `GameScene +0x114` and calls `GameSceneClassLayer::showEventObjTip(...)` with the matching event ID and boolean `true`.

The spatial behavior inside `showEventObjTip` is a separate recovered boundary. In particular, this schedule contract does not decide which `GameSceneObject` ultimately receives `triggerSceneAction()`.

## Project reconstruction

`scene_actions_trigger_schedule.h` reconstructs only the retained accumulator, ordered one-shot bytes, strict thresholds, and one-event-per-invocation behavior. It deliberately returns an event ID instead of dispatching through scene layers, keeping the timed scheduler separate from spatial object selection.

The project boundary rejects non-finite input transactionally so NaN/Inf cannot poison retained state. Finite negative input is not clamped because the native routine performs an unconditional floating-point add with no sign test.

## Remaining boundary

The next integration step is the spatial event-object contract in `GameSceneClassLayer::showEventObjTip(...)` / `GameSceneObject::isContainsRect(...)`, including the invisible `empty.png` fallback hitbox created for the `klhuo-1.png` GameSceneObject. Automatic variant selection remains separate until process-global `lrand48()` seed/order is recovered.
