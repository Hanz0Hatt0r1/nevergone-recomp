# Player action runtime loading

Issue #148 carries the evidence-backed initial player action lookup into the Android offline-scene lifecycle without committing original game resources.

## Runtime trigger

`GameSceneDirectAssetStager.stageIfNeeded()` runs on the `GLSurfaceView` render thread while the project-owned GameLevels render queue is active. It also calls `PlayerActionRuntimeLoader.stageIfNeeded(assetRoot)` before snapshotting the composed render requests.

The loader does nothing until the current GameLevels render queue is ready and an evidence-backed offline Career `1..5` is available.

There are two proven Career sources:

- existing-role entry: the Career attached to the committed `EnterGameWithCid` dispatch;
- fresh-role preservation: the Career retained by `CharacterNameLayer::CretaUI(career)` after `character_name_state::complete_creation()` records the successful CreateCharacter/local-GameLevels handoff.

The existing-role dispatch wins if both retained states happen to be present. Merely selecting a career or opening/submitting CharacterName is not enough for the fresh-role path; the completion boundary must have been reached.

## Imported action resolution

For the resolved Career `N`, `PlayerActionAssetResolver` searches only the app-private imported `filesDir/assets` tree and requires a unique case-insensitive basename match for:

```text
pNN_actions.plist
```

The plist is parsed with external entities and DTD loading disabled. The first bounded runtime action remains the recovered construction state:

```text
action28
```

The action dictionary's `name` is reduced to a safe basename. Device evidence now proves that the original writable data does not require that basename as a standalone file. `ActionDataManager::LoadPlayerActionDataWithFile()` loads `player_NN.actionsData`, and the requested `.actData` payload is an embedded member of that aggregate container.

When a unique `player_NN.actionsData` is present, the resolver validates its bounded record framing and requires exactly one embedded entry with the requested basename. If no player container is present, a unique standalone `.actData` remains supported for extracted/test data.

Resolution fails closed on invalid Career/action keys, duplicate or malformed inputs, missing container entries, duplicate embedded entries, filesystem escape, excessive traversal depth/file count, oversized plist/container data, or invalid entry names.

## Native document store

`player_action_document_store::load_from_assets()` retains the standalone path. `load_embedded_from_assets()` handles the recovered aggregate format:

```text
<ASCII entry name> "\n"
<ASCII decimal payload size> "\n"
<payload bytes>
"\n"
```

The container is bounded to 64 MiB by default, entry names to 512 bytes and entry count to 4096. The parser scans the container without materializing unrelated payloads, requires an exact unique target entry, and copies only that selected payload into the existing `hp_data::Reader` / `enemy_actions_wbg_document::parse()` pipeline.

A failed replacement clears the previous document so stale player action state cannot survive a bad load.

Detailed physical-data observations are recorded in `docs/evidence/player-actionsdata-container-2026-10-12.md`.

## run-336 Career regression

Physical-device `run-336` proved the first-scene spawn itself was ready, but player motion remained at index 0 because the fresh-role preservation route deliberately has no synthetic `EnterGameWithCid` dispatch. #337 resolved Career from the completed CharacterName handoff without inventing a CharacterID.

## run-337 boundary

The next device capture proves that #337 fixed that layer:

```text
GameScene player spawn state
state: ready
player point: 374, 278

GameScene player motion
ready: true
player index: 1
```

Live joystick input also advanced the reconstructed position cursor:

```text
primary state: 0x13
applied steps: 5
normal steps: 5
offset: 39.8605,1.66883
```

The composed renderer nevertheless remained at 132 requests. The accompanying original writable-data capture explains why: `p01_actions.plist/action28` names `p01_i01.actData`, but that resource is embedded inside the 2.9 MiB `player_01.actionsData` aggregate rather than present as a standalone file. All 53 named actions in the supplied `p01_actions.plist` have matching entries in that container.

The runtime now follows that recovered container boundary before asking the existing frame-request/player-atlas pipeline to create the 133rd sprite request.

## Next device proof

A corrected fresh-role run should now advance through:

```text
Career 1 ready
  -> player_01.actionsData
  -> embedded p01_i01.actData
  -> parsed action document
  -> player sprite-frame request
  -> GameScene asset staging requests: 133
  -> player atlas/frame resolution
  -> visible player body
```

If atlas/frame staging fails, diagnostics should now expose that downstream boundary rather than stopping at the missing standalone action resource.

No original plist, `.actData`, `.actionsData`, texture or other proprietary payload is committed to the repository.
