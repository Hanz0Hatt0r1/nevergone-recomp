# Enemy action frames use the scene frame cache

## Native loading path

The `EnemyActionsSystem::updateData()` recovery shows that an ordinary enemy
system (`+0x250 == 0`) formats `enemy%02d/res/%s` from the selected
`ActionFrameData+0x60` string, but does not enter this block's
`CCSpriteFrameCache::addSpriteFramesWithFile()` call. The ReadyUIScene path
separately loads the ordered `GameLevels+0x24` plist list into the shared
`CCSpriteFrameCache` before gameplay rendering. See
[`enemy-actions-update-data-resource.md`](enemy-actions-update-data-resource.md)
and [`game-scene-atlas-frame-resolution.md`](../game-scene-atlas-frame-resolution.md).

The action record's first string is therefore not a guaranteed physical atlas
for ordinary enemy frame lookup. The live renderer must resolve its frame name
against the same ordered shared-cache preload list, which preserves native
first-match behavior when frame names appear in multiple sheets.

## Recompilation change

`game_scene_enemy_render_queue` now leaves
`SpriteCommand::action_frame_atlas_plist` unset for enemy action sprites.
`GameSceneDirectAssetStager` consequently resolves those frame names through
the retained ordered `GameLevels` atlas list, just as it does for native
scene-owned `CCSpriteFrameCache` entries.

On the installed user asset set, `e03_w01_001.png` is present in
`enemy/enemy03/res/e03_03_default.plist` with untrimmed source size 644 by 448,
while enemy03 action data names `e03_default.plist`. The renderer now follows
the native shared-cache lookup. It does not assume that `e03_03_default.plist`
was included in the active scene's preload list; a frame absent from that list
still fails staging rather than being loaded through an unobserved path.
