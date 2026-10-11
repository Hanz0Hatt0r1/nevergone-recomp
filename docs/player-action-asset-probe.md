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

`game_scene_player_frame_request::build()` connects a parsed player action document to the renderer-facing sprite-frame resource contract. The initial `MeActionsSystem::initWithFile()` and `play()` path seeds its own `+0x1a4` frame to zero; it does not use `EnemyActionsSystem +0x294` or its readiness gate. See [`evidence/player-initial-meactions-frame.md`](evidence/player-initial-meactions-frame.md).

A request becomes `ready` only when:

- the selected player action document is loaded and parsed;
- the document contains primary frames;
- `current_frame_1a4` selects an in-range primary record;
- the requested player position is finite;
- the selected `third_string` frame key is non-empty, length-consistent with the serialized `+0x6c` value, bounded to 512 bytes, and contains no path separators or embedded NUL.

On success the bridge emits `game_scene_type0_resource::Kind::kSpriteFrameByName` with the exact recovered frame key. That is the same resource kind consumed by the GameScene atlas discovery, frame-resolution, pixel-staging and GLES texture pipeline. The initial player request is appended to the project-owned scene render queue after its action document and spawn marker are ready.

`tools/game_scene_player_frame_request_smoke.cpp` protects the ready path and fail-closed behavior for a closed readiness gate, empty documents, out-of-range frame state, invalid position, traversal-like frame names, length mismatch and empty frame keys.

## Evidence boundary

The `MeActionsSystem::initWithFile()` and `play()` path selects primary record zero for the first action. `MeActionsSystem::updateData()` requests that record's `+0x68` frame key. The local asset probe exposes this key without committing the payload.

The project-owned path now reaches:

```text
pNN_actions.plist/action28/name
    -> matched user-owned .actData
    -> enemy_actions_wbg_document
    -> MeActionsSystem initial frame zero
    -> validated sprite-frame-by-name request
    -> GameScene atlas staging and player sprite queue
```

The runtime now loads the selected imported action, attaches the validated initial frame to a project-owned player sprite command, and resolves the frame against imported player default atlases. A visible input transition remains downstream of this initial sprite.

No original assets are stored in this repository.
