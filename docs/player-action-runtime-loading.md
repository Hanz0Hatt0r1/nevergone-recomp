# Player action runtime loading

Issue #148 now carries the evidence-backed initial player action lookup into the Android offline-scene lifecycle without committing original game resources.

## Runtime trigger

`GameSceneDirectAssetStager.stageIfNeeded()` already runs on the `GLSurfaceView` render thread while the project-owned GameLevels render queue is active. It now also calls `PlayerActionRuntimeLoader.stageIfNeeded(assetRoot)`.

The loader does nothing until both conditions are true:

- `game_levels_runtime_state` reports a ready current scene render queue;
- `role_selection_state` contains a committed enter dispatch with Career `1..5`.

The Career source is deliberately the committed `EnterGameWithCid` dispatch rather than the current UI selection. This keeps the player resource selector tied to the role that actually crossed the enter-game boundary.

## Imported asset resolution

For the committed Career `N`, `PlayerActionAssetResolver` searches only the app-private imported `filesDir/assets` tree and requires a unique case-insensitive basename match for:

```text
pNN_actions.plist
```

The plist is parsed with external entities and DTD loading disabled. The first bounded runtime action remains the recovered construction state:

```text
action28
```

Its dictionary `name` value is reduced to a basename only for matching. The resolver then requires exactly one imported file with that basename and returns its path relative to `filesDir/assets`.

Resolution fails closed on an invalid Career/action key, missing or duplicate action plist, malformed/oversized plist, missing action name, ambiguous action-data basename, filesystem escape, excessive traversal depth or excessive imported file count.

## Native document store

`player_action_document_store::load_from_assets()` accepts only a relative path beneath `filesDir/assets`, canonicalizes the target, rejects root escape, requires a regular file, applies a 64 MiB default bound and reads the payload into `hp_data::Reader`.

The payload is parsed by the existing `enemy_actions_wbg_document::parse()` implementation. On success the project retains only the project-owned parsed document plus bounded diagnostic metadata. A failed replacement clears the previous document so stale player action state cannot survive a bad load.

`PlayerActionRuntimeLoader` attempts a Career once per active scene/resource generation. `GameSceneDirectAssetStager.invalidate()` also invalidates this loader so replacing imported APK/OBB data forces a fresh resolution and parse.

## Current boundary

The Android path now reaches:

```text
committed role Career
  -> filesDir/assets/**/pNN_actions.plist
  -> action28/name
  -> unique imported .actData
  -> bounded native file load
  -> enemy_actions_wbg_document::Document
```

This intentionally stops before choosing a visible frame. The next #148 step is to initialize the evidence-backed player action runtime state (including the proven readiness/current-frame transition), pass the loaded document through `game_scene_player_frame_request`, and attach the resulting frame to a project-owned player render node at the recovered first-scene entry position.

No original plist, `.actData`, texture or other proprietary payload is committed to the repository.
