# Player action runtime loading

Issue #148 carries the evidence-backed initial player action lookup into the Android offline-scene lifecycle without committing original game resources.

## Runtime trigger

`GameSceneDirectAssetStager.stageIfNeeded()` runs on the `GLSurfaceView` render thread while the project-owned GameLevels render queue is active. It also calls `PlayerActionRuntimeLoader.stageIfNeeded(assetRoot)` before snapshotting the composed render requests.

The loader does nothing until the current GameLevels render queue is ready and an evidence-backed offline Career `1..5` is available.

There are now two proven Career sources:

- existing-role entry: the Career attached to the committed `EnterGameWithCid` dispatch;
- fresh-role preservation: the Career retained by `CharacterNameLayer::CretaUI(career)` after `character_name_state::complete_creation()` records the successful CreateCharacter/local-GameLevels handoff.

The existing-role dispatch wins if both retained states happen to be present. Merely selecting a career or opening/submitting CharacterName is not enough for the fresh-role path; the completion boundary must have been reached.

This distinction is required because the preservation route deliberately does not invent a CharacterID or a synthetic EnterGame dispatch. The original still constructs the primary `MEPlayer` with the selected Career, and the reconstructed CharacterName state already retains that exact integer across the successful local handoff.

## Imported asset resolution

For the resolved Career `N`, `PlayerActionAssetResolver` searches only the app-private imported `filesDir/assets` tree and requires a unique case-insensitive basename match for:

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

## run-336 regression and fix

Physical-device `run-336` proved the first-scene spawn itself was already correct after #336:

```text
GameScene player spawn state
state: ready
matching markers: 1
entry marker applied: yes
player point: 374, 278
```

But the composed renderer still had only 132 requests, not the expected 133, and motion repeatedly reported:

```text
ready: false
spawn generation: 0
player index: 0
```

The same capture showed the fresh-role completion data was live (`selected-career=1`, CharacterName `career=1`, `completion=1`) while `role_selection_state` had no committed EnterGame dispatch. `player_action_runtime_bridge.cpp` was reading only that existing-role dispatch state, so `nativeCurrentOfflineCareer()` returned `0`, `PlayerActionRuntimeLoader` never resolved the player action document, and the player render command was never composed.

The runtime now resolves Career from the completed fresh-role CharacterName state when no valid committed existing-role dispatch exists. A dedicated smoke test locks the priority and fail-closed rules.

## Current device proof

A corrected fresh-role run should now show all of the following together:

```text
Offline/fresh Career = 1
GameScene player spawn state: ready
GameScene player motion: ready, player index 1
GameScene asset staging requests: 133
last static sprites: 133
```

If the action document or player atlas fails after Career resolution, the staging failure advances to that exact boundary rather than being hidden behind player index `0`.

No original plist, `.actData`, texture or other proprietary payload is committed to the repository.
