# SceneActionsSystem::updateData() reflected geometry — 2026-10-12

## Point helpers

Two local Thumb helpers used by the reflected `SceneActionsSystem::updateData()` branch are now decoded directly:

- `0x00351ae8`: construct `lhs - rhs` (`vsub.f32` on X and Y);
- `0x00351abc`: construct `lhs + rhs` (`vadd.f32` on X and Y).

This removes the need to infer the SceneActions mirror from the separate EnemyActions implementation.

## Reflected position

Before the flip branch the routine computes

`base = ((AFD+0x14 + sys+0x244) / sys+0x240,
         (AFD+0x18 + sys+0x248) / sys+0x240)`

and retains that point at `SceneActionsSystem +0x1b0`.

For the reflected path it then computes

`q = (AFD+0x1c / sys+0x240, AFD+0x20 / sys+0x240)`.

The exact helper sequence is:

1. `delta = q - base` via `0x351ae8`;
2. negate only `delta.y`;
3. `position = q + delta` via `0x351abc`.

Therefore the resulting position simplifies to:

`position.x = 2*q.x - base.x`

`position.y = base.y`

This is intentionally **not** replaced with the EnemyActions pivot formula.

## Anchor, rotation and scale

The reflected branch also performs:

- anchor X: `0.5 - (AFD+0x24 - 0.5)` = `1.0 - AFD+0x24`;
- anchor Y: `AFD+0x28`;
- rotation: `-(AFD+0x34)`;
- scale X: `AFD+0x44 * sys+0x240`;
- scale Y: `AFD+0x48 * sys+0x240`;
- final `setFlipX(sys+0x15e)`.

The existing ActionFrameData parser maps AFD `+0x1c/+0x20` to `float_values[6]/[7]`.

## Project boundary

`scene_actions_flip_x_geometry::build()` models only this deterministic geometry. It requires the already-proven reflected flag and rejects zero/non-finite system scale or non-finite frame data. Resource lookup, GL drawing and secondary SceneActions sprites remain separate boundaries.
