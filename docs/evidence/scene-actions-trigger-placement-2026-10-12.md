# SceneActions trigger placement evidence — 2026-10-12

## Scope

This note records the recovered `GameSceneObject::triggerSceneAction()` placement and attachment boundary for the paired `SceneActionsSystem` objects retained by the exact `klhuo-1.png` type-0 branch.

The source binary is the shipped ARMv7 `libcocos2dcpp.so`. No original binary, asset, screenshot, or decompiler dump is committed here.

## Recovered construction boundary

The `klhuo-1.png` branch in `GameSceneObject::initWithData()` creates the pair in this order:

1. `se01_born%02d.actData` via `SceneActionsSystem::createWithFile(file, 0, 1.0f)`, then writes `SceneActionsSystem +0x24e = 0`;
2. `se01_i%02d.actData` via `SceneActionsSystem::createWithFile(file, 0, 1.0f)`, then writes `SceneActionsSystem +0x24e = 1`.

Both are retained in the `GameSceneObject +0xf0` action-system array. This exact-name branch does **not** position either system or add it as a child during `initWithData()`.

## Recovered trigger boundary

`GameSceneObject::triggerSceneAction()` iterates the retained `+0xf0` array in order. For each element it calls `SceneActionsSystem::play()`, derives a local position, then attaches the system to the owning `GameSceneObject` with the serialized object Z value.

For the first two array slots the recovered placement is:

- slot 0 / born mode: `(object_x, object_y + 135.0f)`;
- slot 1 / idle mode: `(object_x, object_y - 45.0f)`.

The source object fields are the already parsed `GameSceneLayerObjectData` values at `+0x24`, `+0x28`, and `+0x38`, represented in the project as `ObjectRecord.first_point_x`, `ObjectRecord.first_point_y`, and `ObjectRecord.trailing_i32`.

The literal words used by the native function are `0x43070000` (`135.0f`) and `0x42340000` (`45.0f`).

## Tick/completion boundary

`GameSceneObject::tick(float)` iterates the same action-system array. Active systems receive `SceneActionsSystem::step(dt)`. Completed systems are stopped and detached. There is a separate special-case transition for another scene-object family; it is not generalized to `klhuo` here.

## Project contract

`scene_action_trigger_plan::build()` reconstructs only the proven two-slot `klhuo` trigger placement/order contract. It deliberately does not:

- fire `triggerSceneAction()` automatically;
- choose the PRNG variant;
- attach or draw either system before the trigger;
- infer additional transform fields from ordinary type-0 sprite branches;
- advance `SceneActionsSystem` playback timing.

This keeps the current visual staging work compatible with the native lifecycle while leaving the trigger event and animation cursor as separate runtime increments.
