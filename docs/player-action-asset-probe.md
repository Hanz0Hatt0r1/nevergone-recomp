# Player action asset probe

Issue #148 now has a native-evidence-backed lookup boundary for the first requested player action:

```text
MEPLAYERSTATE 0x1b -> pNN_actions.plist -> action28 -> name -> MeActionsSystem::creinitWithFile
```

The remaining resource name lives in user-owned game data rather than the base APK currently available to the project. `tools/player_action_asset_probe.py` resolves that boundary locally without committing or copying proprietary resources.

## Usage

For an extracted OBB/assets directory:

```bash
python3 tools/player_action_asset_probe.py /path/to/obb_extracted --career 1
```

For a ZIP-compatible OBB, extraction is not required:

```bash
python3 tools/player_action_asset_probe.py /path/to/main.*.obb --career 1
```

The default key is `action28`. A different evidence-backed key can be selected with `--action actionNN`.

Without a validator, the JSON output contains only:

- the selected Career;
- the located `pNN_actions.plist` path;
- the requested action key;
- that dictionary entry's `name` string;
- paths whose basename matches the named action resource.

The plist probe reads at most 8 MiB, rejects ambiguous duplicate `pNN_actions.plist` files, validates the action-key shape, and does not emit the plist or `.actData` payload.

## Bridge the resolved action into the WBG parser

Build `tools/enemy_actions_wbg_validate.cpp` as documented in `docs/enemy-actions-wbg-validation.md`, then pass it explicitly:

```bash
python3 tools/player_action_asset_probe.py /path/to/main.*.obb \
  --career 1 \
  --action action28 \
  --validator /tmp/enemy_actions_wbg_validate
```

`--validator` is fail-closed: exactly one matching action-data path must exist. For an extracted directory the validator receives that local file directly. For a ZIP-compatible OBB the probe checks the archive entry's declared uncompressed size, rejects entries larger than 64 MiB, materializes only the selected entry in a temporary directory, invokes the validator, then removes the temporary file.

The nested `actdata_validation` report retains only a small allow-list of parser metadata:

- parser/EOF status and byte counts;
- primary record count;
- primary record 0 `third_string` serialized length;
- the same `third_string` bytes hex-encoded as `primary_record0_frame_key_hex`.

The frame key is emitted as hex so arbitrary user-owned resource bytes cannot inject additional line-oriented validator fields. The repository never stores the selected action-data payload.

## Runtime frame request bridge

`game_scene_player_frame_request::build()` now connects a parsed player action document to the existing renderer-facing sprite-frame resource contract. It delegates frame selection to `enemy_actions_runtime_state::apply_update_data_frame_lookup()` and does not manufacture a readiness state or current-frame index.

A request becomes `ready` only when:

- the recovered `flag_26c` readiness gate is open;
- the document contains primary frames;
- `current_frame_294` selects an in-range primary record;
- the requested player position is finite;
- the selected `third_string` frame key is non-empty, length-consistent with the serialized `+0x6c` value, bounded to 512 bytes, and contains no path separators or embedded NUL.

On success the bridge emits `game_scene_type0_resource::Kind::kSpriteFrameByName` with the exact recovered frame key. That is the same resource kind already consumed by the GameScene atlas discovery, frame-resolution, pixel-staging and GLES texture pipeline. The bridge deliberately stops before inventing player attachment/Z-order semantics that have not yet been recovered.

`tools/game_scene_player_frame_request_smoke.cpp` protects the ready path and fail-closed behavior for a closed readiness gate, empty documents, out-of-range frame state, invalid position, traversal-like frame names, length mismatch and empty frame keys.

## Evidence boundary

The existing `EnemyActionsSystem::updateData()` reconstruction proves that a selected primary frame supplies the sprite-frame lookup key from `ActionFrameRecord::third_string`. The local asset probe exposes record 0's key without claiming that record 0 is universally the runtime-selected frame. Runtime selection still depends on the recovered `current_frame_294` state and readiness gate.

The project-owned path now reaches:

```text
pNN_actions.plist/action28/name
    -> matched user-owned .actData
    -> enemy_actions_wbg_document
    -> recovered updateData frame selection
    -> validated sprite-frame-by-name request
    -> existing GameScene atlas pipeline contract
```

The next bounded #148 runtime step is to load the selected user-owned player action document inside the Android offline-scene lifecycle and attach the validated frame request to a project-owned player render node only after the required attachment/Z-order behavior is evidenced. A visible input transition remains downstream of that integration.

No original assets are stored in this repository.
