# Scene-action render-queue handoff

The first-scene construction plan already distinguishes the exact type-0 `klhuo-1.png` branch from ordinary sprite-backed objects. The shipped branch constructs paired `SceneActionsSystem` objects instead of a normal static sprite.

`game_scene_render_queue` now retains those objects as `SceneActionPairCommand` entries rather than reducing them to a count and discarding their identity. Each command preserves:

- render-layer Z index;
- source layer and object indexes;
- the proven `kSceneActionPair` resource classification;
- the layer camera factor;
- the complete parsed source object record.

The source record is intentionally retained without converting it to `Type0SpriteTransform`: the original action-pair branch does not create the ordinary sprite at this boundary, and later `SceneActionsSystem` reconstruction should consume only fields proven for that runtime.

Malformed pair classifications fail closed into the unresolved-object count.

## Next runtime boundary

The next consumer can combine each retained command with `game_scene_type0_action_pair::build()` and the project-owned `trap_actions_document` parser to stage the selected `se01_bornXX.actData` / `se01_iXX.actData` pair from imported user-owned resources. Global PRNG seed/order and SceneActions playback timing remain separate evidence requirements and are not invented by the queue.
