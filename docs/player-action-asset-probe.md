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

## Evidence boundary

The existing `EnemyActionsSystem::updateData()` reconstruction proves that a selected primary frame supplies the sprite-frame lookup key from `ActionFrameRecord::third_string`. The new host bridge therefore exposes record 0's key without claiming that record 0 is universally the runtime-selected frame. Runtime selection still depends on the recovered `current_frame_294` state and readiness gate.

This closes the local tooling gap between:

```text
pNN_actions.plist/action28/name
    -> matched user-owned .actData
    -> enemy_actions_wbg_document
    -> primary record 0 third_string/frame-key metadata
```

The next runtime step is to feed the player action document into the existing `enemy_actions_runtime_state::apply_update_data_frame_lookup()` path with the reconstructed player state, then bind the selected frame key to the player sprite renderer.

No original assets are stored in this repository.
