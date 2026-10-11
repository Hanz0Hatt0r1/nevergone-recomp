# Player atlas request scoping

The first live player frame added by #315 is a `spriteFrameByName()` resource, but it does not share the same preload domain as static GameScene frame lookups.

## Problem

Static GameScene frames are resolved from the ordered plist set recovered from `pvp_scene.glData`. The initial `MeActionsSystem` player frame is resolved from the selected career's `player/PlayerNN_Res/pNN_*_default.plist` resources.

Combining both atlas sets before every `spriteFrameByName()` lookup creates two failure modes:

1. a frame name that is unique inside its own domain can become ambiguous if another domain contains the same key;
2. a player-only frame lookup unnecessarily depends on successful GameScene atlas-list discovery.

The original runtime does not merge those preload scopes for the lookup boundary reconstructed here: the player defaults are loaded by the MEPlayer path, while GameScene preloads use the scene atlas list.

## Request contract

`game_scene_direct_asset_requests` now exposes three staging kinds:

```text
0  direct file
1  GameScene spriteFrameByName
2  player spriteFrameByName
```

A render command is classified as player-frame kind only when it carries the `player_body_transform` marker introduced by the initial player render queue. The request kind participates in the snapshot revision hash, so changing atlas domains with the same frame-name string invalidates the staged texture snapshot.

JNI preserves the distinct kind when exposing the immutable request snapshot to Java.

## Java staging

`GameSceneDirectAssetStager` now resolves atlas domains independently:

```text
kind 1 -> GameSceneAtlasListDiscovery -> GameSceneAtlasResolver.resolve(...)
kind 2 -> loaded player Career -> GameSceneAtlasResolver.resolvePlayerDefaults(...)
```

Each frame is passed only the atlas list for its own request kind. Direct-file requests are unchanged. The decoded-atlas bitmap cache remains shared by canonical texture path, so this separation does not duplicate a texture decode if two valid domains happen to point at the same underlying PNG.

Player-frame failures are reported as `player-sprite-frame-by-name` in resource-name-free staging diagnostics.

## Result

The initial player renderer no longer broadens static scene frame resolution and no longer requires `pvp_scene.glData` atlas discovery merely because a player frame is present. This keeps the #315 live renderer path bounded to the resource domains recovered for each caller.

No proprietary plist, PNG or action-data payload is stored in the repository.
