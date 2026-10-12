# Player `actionsData` container evidence (2026-10-12)

## Provenance

This note records bounded observations from a user-supplied copy of the original Android app's writable player data. No original payload is committed to this repository.

Observed files:

- `player_01.actionsData`: 2,945,850 bytes, SHA-256 `c71513095b38d0d2e9de3cbacb5204deadfcd77d949093638c4fd67f2eee2a99`;
- `p01_actions.plist`: 9,084 bytes, SHA-256 `f0cfba7dba54a17c055d1894823a103956220f0f9aaf4c71c25833aa5b96c514`.

The original runtime had already been observed calling `ActionDataManager::LoadPlayerActionDataWithFile(int, char const*)` with `player_01.actionsData` after hero selection.

## Container framing

A complete bounded scan consumes the `player_01.actionsData` file exactly as 125 records with this framing:

```text
<ASCII entry name> "\n"
<ASCII decimal payload size> "\n"
<payload bytes>
"\n"
```

There are no bytes left after the 125th record when the final record terminator is consumed.

The first record is `p01_a01a.actData` with a declared payload size of 24,252 bytes. This is consistent with `actionsData` being an aggregate of action-data payloads rather than one `EnemyActionsData` document.

## `action28` boundary

The supplied `p01_actions.plist` maps:

```text
action28 -> p01_i01.actData
```

`p01_i01.actData` is not present as a standalone file in the tested writable asset tree. It is present exactly once inside `player_01.actionsData`:

- record index: 46 (zero-based);
- record header starts at byte 1,063,289;
- payload starts at byte 1,063,311;
- payload length: 11,682 bytes;
- payload ends at byte 1,074,993.

The payload begins with the same binary field shape expected by the recovered action-data parser (`0x69` little-endian first word, then the following recovered scalar fields); the runtime parser remains the authority for full validation.

## Cross-check against the plist

The supplied plist contains 53 `actionNN` dictionaries with a `name` string. Reducing those names to basenames and comparing them with the 125 container entry names gives:

```text
plist action names checked: 53
matching actionsData entries: 53
missing from actionsData: 0
```

This resolves the previous apparent `p01_i01.actData` resource gap: the file is an embedded member of the player action container loaded by `ActionDataManager`, not a required standalone asset.

## Reconstruction consequence

The preservation runtime should follow the recovered boundary:

```text
pNN_actions.plist / actionNN / name
    -> player_NN.actionsData
    -> exact embedded entry by basename
    -> bounded extracted payload in memory
    -> existing enemy_actions_wbg_document parser
    -> MeActions frame request
```

The container parser must remain fail-closed on malformed framing, oversized data, duplicate target entries, missing target entries, invalid entry names and filesystem escape. The original container or extracted payload must not be committed.
