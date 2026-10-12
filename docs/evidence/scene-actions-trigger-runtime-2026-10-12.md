# SceneActions trigger runtime evidence — 2026-10-12

## Native trigger gate

`GameSceneObject::triggerSceneAction()` begins by reading byte `GameSceneObject +0x105`. If it is already non-zero, the function returns without replaying the retained action array.

If the retained `GameSceneObject +0xf0` array is empty, native writes `+0x105 = 1` and returns. Otherwise it iterates the array, calls `SceneActionsSystem::play()` on each entry, applies the recovered per-slot position for the first two entries, attaches each system to the owning `GameSceneObject`, and after the loop reaches the same `+0x105 = 1` completion path.

This establishes a one-shot trigger boundary independent of animation completion.

## Recovered call-site

A direct caller is `GameSceneClassLayer::showEventObjTip(...)`. It iterates scene objects, evaluates `GameSceneObject::isContainsRect(...)` against the relevant object rectangle, and calls `triggerSceneAction()` only when the intersection test succeeds.

Therefore visual readiness or scene construction alone is not evidence for starting the `klhuo` action pair. The trigger is event/spatial-condition driven.

## Project runtime

`scene_action_trigger_runtime::Runtime` models only the proven one-shot trigger gate and command emission:

- a valid two-slot `scene_action_trigger_plan::Plan` emits born then idle;
- both emitted commands request `play` and child attachment;
- a successful trigger is consumed exactly once;
- repeated calls do not emit a second start;
- invalid slot/mode structure fails without consuming the trigger;
- `reset()` is reserved for a recreated/reloaded GameSceneObject lifetime.

The runtime does not yet model the `showEventObjTip` rectangle condition itself, `SceneActionsSystem::step(dt)`, frame advancement, completion, stop/detach, or the special non-klhuo branch in `GameSceneObject::tick()`.
