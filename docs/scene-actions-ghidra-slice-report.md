# SceneActions Ghidra slice report

`tools/ghidra/ExportSceneActionTriggerSlice.java` exports the narrow local-only evidence needed for the two remaining SceneActions runtime gaps:

- spatial trigger geometry around `showEventObjTip` / `isContainsRect`;
- the native `TrapActionsData +0xbc` count representation consumed by `SceneActionsSystem::waUpdate(float)`.

Raw Ghidra output is local evidence and must not be committed. After exporting it, generate a compact review report instead:

```bash
python3 tools/scene_actions_slice_report.py \
  build/scene-actions-trigger-slice.txt \
  --output build/scene-actions-trigger-report.md
```

For machine-readable output:

```bash
python3 tools/scene_actions_slice_report.py \
  build/scene-actions-trigger-slice.txt \
  --json \
  --output build/scene-actions-trigger-report.json
```

The report records target resolution, short lexical candidates for the spatial trigger, `+0xbc`/decimal-188 references, and nearby compare/branch instructions. It intentionally does **not** infer rectangle semantics, mutation direction, count/index equivalence, sentinel layout, or global PRNG ordering.

By default the tool exits non-zero if any expected target is absent or unresolved. `--allow-incomplete` is only for inspection of partial exports; incomplete output is not sufficient evidence for changing runtime behavior.
