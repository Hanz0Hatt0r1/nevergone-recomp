# User-owned resource index

The project uses original APK/OBB data supplied by the user at runtime or during local reverse-engineering. Proprietary game assets are not stored in this repository.

A local extracted OBB tree may contain resource families such as:

- `gamescene` (`gs_list`, `scene_data_files`, `gs_actions`, `gs_state_machine`, image/event resources);
- `player`;
- `enemy` and `NPC`;
- `weapons` and `equips`;
- `config` and `DialogueConfig`;
- `sound`;
- `shader` / `mat`;
- `Guide`, `pets`, `other`, and related content.

The exact tree remains user-owned and outside source control.

## Generate an index

Use the deterministic metadata indexer against a local extracted resource root:

```bash
python3 tools/resource_index.py /path/to/extracted/assets \
  --json build/resource-index.json \
  --csv build/resource-index.csv
```

The full index contains only metadata:

- relative path;
- inferred subsystem;
- extension;
- byte size;
- optional SHA-256 of the local file.

It does not copy file contents.

## Shareable modes

For coordination or diagnostics, prefer an aggregate summary:

```bash
python3 tools/resource_index.py /path/to/extracted/assets \
  --no-content-hash \
  --summary-only
```

If per-file identity is required without exposing filenames, hash paths:

```bash
python3 tools/resource_index.py /path/to/extracted/assets \
  --path-mode hash \
  --json build/resource-index-hashed.json
```

Full path inventories should normally stay local. Commit only deliberately reviewed summaries or synthetic fixtures.

## How the index is used

The index is a coordination layer, not a source of truth for behavior. It helps answer:

- which original data families are available;
- which files have already been mapped to a parser/runtime consumer;
- whether two reverse-engineering tasks are examining the same resource;
- whether a local resource set changed between experiments;
- which subsystem owns the next playable-path blocker.

Behavioral claims still require evidence documented using `docs/reverse-engineering-evidence-template.md`.

## Recommended local status table

For active work, maintain a local derived table with these columns:

```text
path-or-hash
subsystem
extension
size
sha256
parser-status
runtime-consumer
evidence-status
playable-path-checkpoint
notes
```

Only the first five columns are generated automatically. The remaining fields represent project knowledge and should be promoted into repository documentation once they are evidence-backed.
