# Initial player GLES quad boundary

Issue #148 now has enough evidence-backed state to flatten the first visible `MEPlayer -> MeActionsSystem -> body sprite` branch into the project-owned GLES coordinate model without assuming a centered anchor.

## Inputs

`game_scene_initial_player_sprite_request` supplies the two independent transforms recovered from the original ARMv7 binary:

- the `MeActionsSystem` node position, equal to the final first-scene `MEPlayer::getPlayerPos()` value;
- the primary body sprite's local frame position, anchor, scale, rotation and flip-X state from primary ActionFrameData frame 0.

The frame request also carries the exact `spriteFrameByName()` key for that same frame.

Java's existing atlas pipeline reconstructs TexturePacker frames as standalone transparent images at `spriteSourceSize`, restoring trimmed pixels and offsets into the original untrimmed rectangle. The staged texture width/height therefore matches the geometry on which the original CCSprite anchor operates.

## Quad construction

For a staged source-size texture `(W,H)` and body anchor `(ax,ay)`, local bounds around the anchor are:

```text
left   = -ax * W
right  = (1-ax) * W
bottom = -ay * H
top    = (1-ay) * H
```

Each corner is then processed in original Cocos order:

```text
anchor-relative corner
    -> body scale X/Y
    -> clockwise body rotation
    -> body local position
    -> MeActionsSystem / MEPlayer world point
    -> 1136x640 ExactFit clip coordinates
```

Positive Cocos rotation uses the same clockwise convention already reconstructed for static GameScene sprites. Physical GLES surface dimensions gate validity but do not alter clip fractions under the original `setDesignResolutionSize(1136,640,ExactFit)` policy.

`flipX` swaps U coordinates on the full reconstructed source-size texture. Because transparent trim padding has already been restored, this also mirrors the original TexturePacker offset rather than mirroring only the packed crop.

## Flattened draw order

The original GameScene attaches its scene-layer children first, then attaches `BattleManager` at GameScene Z=7. The player branch lives below that manager. For the current flattened renderer boundary, the primary player draw therefore belongs:

```text
all drawable static layer-Z <= 7 commands
    -> initial player body
    -> drawable static layer-Z >= 8 commands
```

Same-Z arrival order is preserved: layer 7 remains before the later-added BattleManager.

`game_scene_initial_player_geometry::static_draw_count_before_player()` returns the static texture draw count at that insertion boundary while ignoring malformed/non-drawable static commands.

## Failure behavior

The quad builder fails closed when the composed request is not ready, the nested frame request/resource is missing, the sprite transform is absent, dimensions are non-positive, or any required coordinate/transform becomes non-finite.

The helper does not yet allocate or upload a player texture. It deliberately consumes only staged texture dimensions and returns a `game_scene_direct_geometry::Quad` compatible with the existing GLES renderer.

## Next integration step

The remaining first-frame renderer work is now an asset-lifecycle problem rather than a geometry problem:

1. expose the ready player `spriteFrameByName()` request alongside the current static GameScene asset snapshot;
2. stage that frame through the existing atlas resolver/pixel reconstruction path under a collision-free player texture key;
3. upload it through the existing native texture cache;
4. submit the player quad at the recovered Z=7 insertion boundary.

No proprietary image, plist or action-data payload is committed.
