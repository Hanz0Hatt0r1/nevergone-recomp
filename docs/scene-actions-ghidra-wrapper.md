# SceneActions Ghidra wrapper

`tools/run_scene_actions_ghidra_slice.py` runs the bounded local Ghidra export and immediately converts it into compact reports through `tools/scene_actions_slice_report.py`.

Example:

```bash
python3 tools/run_scene_actions_ghidra_slice.py \
  --ghidra /opt/ghidra \
  --project-dir /path/to/ghidra_projects/nevergone \
  --project-name nevergone \
  --program '/com.hippiegame.nevergone.apk/lib/armeabi-v7a/libcocos2dcpp.so'
```

Default outputs are written under `build/scene-actions-ghidra/`:

- `scene-actions-trigger-report.md`;
- `scene-actions-trigger-report.json`.

The raw Ghidra slice is deleted after both compact reports pass validation. Add `--keep-raw` only when local inspection of the bounded raw evidence is required. Raw decompiler output must not be committed.

Use `--dry-run` to print the exact `analyzeHeadless` and report commands without requiring Ghidra or a project directory. The wrapper uses `-noanalysis`: it expects the selected program to have already been analyzed in the existing Ghidra project.
