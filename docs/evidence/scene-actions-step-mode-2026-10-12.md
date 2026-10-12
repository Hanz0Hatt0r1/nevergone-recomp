# SceneActions step completion modes — 2026-10-12

## Scope

This note records the recovered completion branch in `SceneActionsSystem::step(float)` for the two `+0x24e` modes already proven for the `klhuo-1.png` pair.

No original binary, asset, screenshot, or decompiler output is committed here.

## Recovered behavior

`step(dt)` first checks whether the system is active, executes the normal `waStep(dt)` path, then tests `isDone()` (`SceneActionsSystem +0x1a8`).

When the system is not done, `step()` returns normally.

When the system is done:

- `+0x24e == 0`: return with the system still complete;
- `+0x24e != 0`: call `play()`, then immediately execute `waStep(dt)` again.

The exact `klhuo-1.png` construction branch assigns:

- born slot: `+0x24e = 0`;
- idle slot: `+0x24e = 1`.

Therefore the born action is finite while the idle action restarts/loops after reaching its completion boundary.

## Project contract

`scene_actions_step_mode::on_completed()` accepts only the two modes proven for the retained klhuo pair:

- mode 0 -> remain done;
- mode 1 -> restart via `play()` and step again with the current `dt`.

Other mode bytes fail closed rather than generalizing the native non-zero branch to unobserved project consumers.

This contract intentionally does not reconstruct frame advancement inside `waUpdate(float)`, action-data transforms from `updateData()`, or the owning `GameSceneObject::tick()` detach path. Those remain separate increments.
