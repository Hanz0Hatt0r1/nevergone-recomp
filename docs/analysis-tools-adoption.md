# Analysis/validation tools adopted from the external proposal

This change deliberately adopts the useful ideas from the proposed eight-tool pack without treating the supplied scripts as authoritative. The repository already has stricter runtime-evidence and reverse-engineering tooling, so the new utilities integrate with those contracts instead of creating parallel mock gates.

## Adopted

### `tools/device/p7_p10_contiguous_proof.py`

Primary real-device acceptance harness for the current P7-P10 critical path. Run it **after the recomp is already foregrounded on the live first GameLevels scene**:

```bash
python3 tools/device/p7_p10_contiguous_proof.py
# or, with more than one adb device:
python3 tools/device/p7_p10_contiguous_proof.py --device-id SERIAL
```

The harness reuses `tools/android_game_scene_render_evidence.sh` for P7 and `tools/android_player_input_runtime_evidence.sh` for P8/P9. It then reads and validates the project-owned `files/nevergone-recomp/minimal-save-v1.bin` sidecar, force-stops the process, starts `org.nevergone.recomp/.MainActivity`, retains a second renderer/runtime snapshot, and requires the sidecar scene/player/motion tuple to match the post-restart runtime state. It also requires a real PID change and preserves failure artifacts instead of emitting a manual-only PASS.

Output defaults to `evidence/android-p7-p10-<UTC>/` and includes `summary.json`, `proof.md`, the nested P7/P8/P9/P10 evidence directories and sidecar snapshots.

### `tools/render/render_diff.py`

Frame comparison for first-scene visual debugging:

```bash
python3 tools/render/render_diff.py \
  --original /path/to/original.png \
  --recomp /path/to/recomp.png \
  --out evidence/render-diff
```

Requires Pillow and NumPy. Unlike the proposed script, it does not silently resize both inputs to `1136x640`. Geometry mismatch fails by default; explicit crops or `--resize` are required. The report includes global SSIM-like similarity, MAE, RMSE, p95/max error, changed-pixel ratio, raw diff and amplified heatmap. Optional thresholds (`--min-global-ssim`, `--max-mae`, `--max-changed-ratio`) make it usable as a local gate without pretending one hard-coded SSIM threshold is universally valid across devices/compositors.

### `tools/validation/asset_coverage.py`

Static lower-bound coverage check for a user-local decoded asset tree:

```bash
python3 tools/asset_decoder.py /path/to/com.hippiegame.nevergone.apk \
  --output build/decoded-assets
python3 tools/validation/asset_coverage.py \
  --decoded-dir build/decoded-assets \
  --fail-on-missing
```

It scans quoted Lua references to `.actData`, `.plist`, `.png`, `.hpc`, `.csv`, and `.lua`, resolves exact or unique suffix paths, and distinguishes missing from ambiguous references. There is no hard-coded `p01_i01.actData` fallback: if that asset is required, it must be discovered from the actual decoded scripts/metadata. Reports contain paths/status only, not proprietary script bodies or assets.

### `tools/build/arm64_sanity_check.py`

Static complement to the existing 16 KiB runtime smoke:

```bash
./gradlew -p android :app:assembleDebug
python3 tools/build/arm64_sanity_check.py
```

The checker inspects `lib/arm64-v8a/*.so` in the APK with `readelf`, requires AArch64/ELF64, parses every `PT_LOAD` alignment and requires at least `0x4000`, and rejects a dynamic dependency on `libcocos2dcpp.so`. This is intentionally separate from `tools/android_16k_runtime_smoke.sh`, which proves the runtime page size on the dedicated emulator.

## Not adopted as supplied

- `controls_divergence_tracer.py`: the supplied version reduces the current Controls work to a hard-coded count of 78 and a set difference over two ad-hoc log formats. The repository already has provenance-aware receiver/alias/vtable audits under `tools/ml/`; a second weaker truth source would be harmful.
- `sceneactions_dynamic_tracer.py`: the supplied implementation only greps text lines containing `PRNG`, `rand`, `timed`, `spatial`, or `parent`; it does not perform dynamic tracing or prove parent geometry/PRNG semantics.
- `enemy_actions_validator.py`: the supplied implementation checks only `len >= 16` and one little-endian integer at offset 4. It does not exercise the reconstruction's bounds-checked EnemyActions/TrapActions parsers, so calling files "valid" from that test would create false confidence.
- `dispatch_ranker_dashboard.py`: the supplied dashboard is populated entirely from hard-coded mock counts/candidates. The current ML pipeline already emits real OOF, quarantine, receiver and routing evidence; no mock dashboard is added.
- `.github/workflows/analysis-tools.yml`: no generic workflow is added for this pack. The highest-value proof requires a real foreground Android device, asset coverage requires user-local copyrighted inputs, and render diff has optional scientific-image dependencies. Static ARM64 audit is safe for CI once attached to a workflow that actually builds the production ARM64 APK; adding an isolated workflow without that artifact would be noise rather than a gate.

## Local verification performed before adoption

- all four Python files compile with `python3 -m py_compile`;
- render diff returns global similarity `1.0`, zero MAE and zero changed pixels for an identical retained 2340x1080 Never Gone frame;
- asset coverage synthetic smoke distinguishes a present `.actData` reference from a missing `.plist` reference;
- ARM64 audit rejects the supplied original Never Gone APK because that APK contains only `armeabi-v7a` libraries, which is the expected result for an ARM64-specific gate.

The contiguous device proof still requires execution on an attached device after the current build reaches the live first GameLevels scene; the tool itself does not fabricate or pre-approve that evidence.
