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

The JSON output contains only:

- the selected Career;
- the located `pNN_actions.plist` path;
- the requested action key;
- that dictionary entry's `name` string;
- paths whose basename matches the named action resource.

The probe reads at most 8 MiB for the plist, rejects ambiguous duplicate `pNN_actions.plist` files, validates the action-key shape, and does not emit the plist or `.actData` payload.

## Next reconstruction step

Once `action28.name` is known, feed the matching user-owned `.actData`/WBG payload through the existing `enemy_actions_wbg_document` parser. For its first selected primary frame, the already recovered `EnemyActionsSystem::updateData()` contract requests `ActionFrameRecord::third_string` through `CCSpriteFrameCache::spriteFrameByName()`. This gives the next bounded bridge from initial player action resource to the first visible sprite-frame key.

No original assets are stored in this repository.
