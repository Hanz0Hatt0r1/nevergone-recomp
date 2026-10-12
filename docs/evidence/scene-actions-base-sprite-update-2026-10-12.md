# SceneActionsSystem::updateData() base sprite evidence — 2026-10-12

## Native source boundary

The original ARMv7 binary exports `SceneActionsSystem::updateData()` at `0x003525a1`.

The primary non-flipped sprite path performs the following operations for the current primary ActionFrameData (`TrapActionsData +0x88[frame_index]`):

1. load the sprite-frame plist from ActionFrameData `+0x60` into `CCSpriteFrameCache`;
2. resolve the sprite frame named by ActionFrameData `+0x68`;
3. call the primary sprite's display-frame setter;
4. compute position X as `(AFD+0x14 + SceneActionsSystem+0x244) / SceneActionsSystem+0x240`;
5. compute position Y as `(AFD+0x18 + SceneActionsSystem+0x248) / SceneActionsSystem+0x240`;
6. pass that point through the CCNode virtual at `+0x54`, matching the recovered `setPosition` slot;
7. build `(AFD+0x24, AFD+0x28)` and pass it through the CCNode virtual at `+0x6c`, matching `setAnchorPoint`;
8. call `setScaleX()` with `ActionFrameData +0x44 * SceneActionsSystem +0x240`;
9. call `setScaleY()` with `ActionFrameData +0x48 * SceneActionsSystem +0x240`;
10. call `setRotation()` using ActionFrameData `+0x34`;
11. call `setFlipX()` from `SceneActionsSystem +0x15e`.

The shared ActionFrameData parser already establishes the serialized mapping used by this plan:

- `+0x14 -> float_values[0]`;
- `+0x18 -> float_values[1]`;
- `+0x24 -> float_values[2]`;
- `+0x28 -> float_values[3]`;
- `+0x34 -> float_values[9]`;
- `+0x44 -> float_values[10]`;
- `+0x48 -> float_values[11]`;
- `+0x60 -> first_string`;
- `+0x68 -> third_string`.

## Correction note

The first project-owned draft mislabeled the `+0x6c` virtual call as position and therefore treated AFD `+0x24/+0x28` as position. Cross-checking the adjacent recovered EnemyActions CCNode virtual layout and the complete SceneActions control flow proves that call is the anchor-point setter. The project helper and regression now preserve the actual position/anchor split above.

## Reflected branch

When `SceneActionsSystem +0x15e` is non-zero, `updateData()` enters a distinct geometry branch before applying the common scale/rotation/flip tail. That branch uses additional system points and frame values and is not equivalent to simply negating the non-flipped X coordinate.

`scene_actions_base_sprite_update::build_non_flipped()` therefore rejects reflected input instead of inventing that geometry.

## Project boundary

The project-owned plan records only deterministic renderer-facing values for the non-flipped branch: atlas plist, frame name, position, anchor, rotation and final X/Y scale. It rejects empty resource names, zero system scale and non-finite inputs.

This increment does not resolve pixels, perform GL calls, advance `waUpdate`, or implement the reflected-position branch. Existing SceneActions asset staging remains responsible for turning the requested atlas/frame into user-imported pixels.
