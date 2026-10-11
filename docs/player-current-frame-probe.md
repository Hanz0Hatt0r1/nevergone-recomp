# Initial player current-frame evidence probe

Issue #148 now has a corrected player-specific current-frame contract. Earlier probe revisions targeted `+0x294`, which is an `EnemyActionsSystem` frame field and must not be reused for the first player `MeActionsSystem` action.

Instruction-level evidence from the original Never Gone 1.0.9 ARMv7 `libcocos2dcpp.so` (SHA-256 `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`) identifies the player fields as:

- `MeActionsSystem +0x1a4`: current frame;
- `MeActionsSystem +0x1a8`: previous frame.

`MeActionsSystem::initWithFile()` writes `0` to `+0x1a4` and `-1` to `+0x1a8`. `MeActionsSystem::play()` repeats those values immediately before selecting `objectAtIndex(+0x1a4)` and calling `updateData()`. The initial player action therefore starts at primary frame index 0.

## One-command headless regression probe

For an existing analyzed Ghidra project, the repository wrapper rechecks stores to the corrected player current-frame field:

```bash
tools/ghidra/run_player_current_frame_probe.sh \
  /path/to/ghidra \
  /path/to/ghidra-projects \
  NevergoneProject \
  libcocos2dcpp.so
```

An optional fifth argument selects the output directory. The default is:

```text
artifacts/player-current-frame-probe/
```

The wrapper uses the existing project with `-process` and `-noanalysis`; it does not import or modify original binaries. It writes only bounded derived TSV metadata and its validated summary.

## Ghidra field-store candidate export

`tools/ghidra/ExportFieldStoreCandidates.java` performs a bounded High P-code scan of matching functions. The wrapper invokes the equivalent of:

```text
ExportFieldStoreCandidates.java "MeActionsSystem" 0x1a4 /tmp/player-current-frame-stores.tsv
```

The generic exporter follows STORE destination-address dependencies and emits a row only when the chain contains an exact `PTRSUB` or `INT_ADD` with the requested constant offset. The TSV contains derived metadata only: containing function identity, STORE P-code identity, requested field offset, matching address-expression metadata and bounded value-dependency/constant metadata. It does not export instruction bytes, disassembly, decompiler C text, original strings, resources or assets.

## Validation

The headless wrapper runs the same validator automatically. It can also be invoked directly:

```bash
python3 tools/ghidra/summarize_field_store_candidates.py \
  /tmp/player-current-frame-stores.tsv --field-offset 0x1a4
```

The validator fails closed on schema errors, wrong field offsets, unsupported address opcodes, contradictory constant metadata and duplicate STORE identities.

The generic candidate summary remains supporting metadata rather than semantic proof by itself. The semantic association is supplied by the bounded instruction path recorded in `docs/evidence/me-actions-initial-frame.md`: the zero stores occur in `MeActionsSystem::initWithFile()` and `MeActionsSystem::play()`, and `play()` immediately uses `+0x1a4` to select the primary frame before `updateData()`.

## Runtime consequence

The previous frame blocker is closed for the initial action:

```text
MeActionsSystem init/play
    -> current frame +0x1a4 = 0
    -> primary_records[0]
    -> ActionFrameData +0x68
    -> spriteFrameByName(frame key)
```

`game_scene_player_frame_request` now consumes this player-specific state and no longer depends on `EnemyActionsSystem +0x26c/+0x294` readiness/frame semantics.
