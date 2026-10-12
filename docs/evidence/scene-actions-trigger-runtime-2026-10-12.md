# SceneActions trigger runtime evidence — 2026-10-12

## Native trigger gate

`GameSceneObject::triggerSceneAction()` begins by reading byte `GameSceneObject +0x105`. If it is already non-zero, the function returns without replaying the retained action array.

If the retained `GameSceneObject +0xf0` array is empty, native writes `+0x105 = 1` and returns. Otherwise it iterates the array, calls `SceneActionsSystem::play()` on each entry, applies the recovered per-slot position for the first two entries, attaches each system to the owning `GameSceneObject`, and after the loop reaches the same `+0x105 = 1` completion path.

This establishes a one-shot trigger boundary independent of animation completion.

## Recovered call-site

A direct caller is `GameSceneClassLayer::showEventObjTip(...)`. It iterates scene objects, evaluates `GameSceneObject::isContainsRect(...)` against the relevant object rectangle, and calls `triggerSceneAction()` only when the intersection test succeeds.

Therefore visual readiness or scene construction alone is not evidence for starting the `klhuo` action pair. The trigger is event/spatial-condition driven.

## Project runtime

`scene_action_trigger_runtime::Runtime` models the proven one-shot trigger gate and born/idle command emission:

- a valid two-slot `scene_action_trigger_plan::Plan` emits born then idle;
- both emitted commands request `play` and child attachment;
- a successful trigger is consumed exactly once;
- repeated calls do not emit a second start;
- invalid slot/mode structure fails without consuming the trigger;
- `reset()` is reserved for a recreated/reloaded GameSceneObject lifetime.

The higher-level SceneActions runtime now composes that gate with additional recovered boundaries:

- `scene_actions_active_playback` reconstructs `play()` state and `waUpdate(dt)` frame advancement;
- the terminal transition completes born mode and loops idle mode to frame zero;
- frame-indexed imported pixels are staged against the retained TrapActions document;
- renderer-ready frames preserve recovered parent placement and child Z;
- the GLES compositor renders submitted born/idle frames;
- `scene_actions_current_scene_binding` accepts exactly one retained `klhuo-1.png` pair from the current GameLevels render queue and rejects ambiguous multiple-pair scenes;
- `scene_actions_live_runtime` exposes an explicit current-scene start/tick/submit boundary for an already staged variant.

## Remaining trigger boundary

The project still does **not** automatically fire the trigger. The exact `showEventObjTip` / `GameSceneObject::isContainsRect(...)` rectangle computation has not yet been reconstructed into clean-room runtime code, so invoking SceneActions merely because the scene or assets are ready would be unsupported.

Until that spatial/event condition is recovered, callers must treat live SceneActions start as the explicit equivalent of an already-confirmed native trigger event. Global `lrand48()` ordering for selecting the klhuo variant also remains a separate unresolved boundary.
