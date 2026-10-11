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

`game_scene_player_frame_request::build()` connects a parsed player action document to the existing renderer-facing sprite-frame resource contract using the **player-specific** `MeActionsSystem` runtime state.

Instruction-level evidence now proves that both `MeActionsSystem::initWithFile()` and `MeActionsSystem::play()` initialize current frame `+0x1a4` to `0` and previous frame `+0x1a8` to `-1`. `play()` immediately selects primary frame 0 and calls `updateData()`. The latter reads the current frame from `+0x1a4` and resolves the selected `ActionFrameData +0x68` through `CCSpriteFrameCache::spriteFrameByName()`.

This is intentionally separate from `EnemyActionsSystem`: the earlier bridge incorrectly reused EnemyActions runtime offsets `+0x26c/+0x294`. Those fields do not define the player `MeActionsSystem` playback cursor.

A request becomes `ready` only when:

- the parsed document contains primary frames;
- `current_frame_1a4` selects an in-range primary record;
- the requested player position is finite;
- the selected `third_string` frame key is non-empty, length-consistent with the serialized `+0x6c` value, bounded to 512 bytes, and contains no path separators or embedded NUL.

On success the bridge emits `game_scene_type0_resource::Kind::kSpriteFrameByName` with the exact recovered frame key. That is the same resource kind already consumed by the GameScene atlas discovery, frame-resolution, pixel-staging and GLES texture pipeline.

`tools/game_scene_player_frame_request_smoke.cpp` protects the recovered initial state `(current=0, previous=-1)`, ready frame-0 path and fail-closed behavior for empty documents, negative/out-of-range frame state, invalid position, traversal-like frame names, length mismatch and empty frame keys.

## Position and attachment boundary

The first player point is also now more precise. `MEPlayer` retains its construction/bootstrap point at `+0x240` and the later `setPlayerPos()` argument at `+0x248`; `MEPlayer::getPlayerPos()` and `setPlayerPos()` both use their sum as the active action-node position.

For the primary offline player the bootstrap point is `(0.5 * win_width, 0.7 * win_height)`. The type-6 scene-entry transform is therefore a scene offset, not the final visible point by itself. `game_scene_player_entry_markers::resolve_offline_player_position()` composes the two values and uses the last matching type-6 marker, matching the original loading loop's repeated `setPlayerPos()` calls.

The active `MeActionsSystem` is attached to `MEPlayer` with child Z `2`, while `BattleManager` is attached to `GameScene` with Z `7`. These facts narrow the eventual renderer insertion point without inventing an arbitrary player Z order.

Detailed addresses and field evidence are recorded in `docs/evidence/me-actions-initial-frame.md`.

## Evidence boundary

The project-owned path now reaches:

```text
pNN_actions.plist/action28/name
    -> matched user-owned .actData
    -> enemy_actions_wbg_document parser
    -> MeActions initial state (frame 0)
    -> ActionFrameData +0x68 frame key
    -> validated sprite-frame-by-name request
    -> recovered first-scene visible player position
    -> known MEPlayer/MeActions and GameScene/BattleManager attachment Z values
```

The next bounded #148 visual step is to recover the MeActions main-sprite local transform/facing behavior applied around `spriteFrameByName()` and then feed the resulting player request through the existing atlas staging/GLES path. A live input transition remains downstream of that integration.

No original assets are stored in this repository.
