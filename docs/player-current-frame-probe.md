# Initial player current-frame evidence probe

**Resolution:** The probe's `EnemyActionsSystem +0x294` results do not apply to
the initial `MeActionsSystem` player action. See
[`evidence/player-initial-meactions-frame.md`](evidence/player-initial-meactions-frame.md)
for the corrected `MeActionsSystem +0x1a4` seed and runtime path.

Issue #148 has project-owned runtime state for the first-scene player position and a parsed initial `action28` document, but it still deliberately does not force `EnemyActionsSystem + 0x294` (`current_frame_294`) to zero.

Existing evidence proves that `EnemyActionsSystem::loadingTex()` sets the visual readiness byte at `+0x26c` to `1`. Other recovered routines write `+0x294` later in action playback, including `showActionLastFrame()` and waUpdate completion/reset paths. Those writes do not establish the initial frame selected for the first `MeActionsSystem` action.

## One-command headless run

For an existing analyzed Ghidra project, the repository wrapper runs the exporter and validator in one command:

```bash
tools/ghidra/run_player_current_frame_probe.sh \
  /path/to/ghidra \
  /path/to/ghidra-projects \
  NevergoneProject/com.hippiegame.nevergone.apk/lib/armeabi-v7a \
  libcocos2dcpp.so
```

An optional fifth argument selects the output directory. The default is:

```text
artifacts/player-current-frame-probe/
```

The wrapper uses the existing project with `-process` and `-noanalysis`; it does not import or modify original binaries. It writes only the bounded derived TSV and its validated summary.

## Ghidra field-store candidate export

`tools/ghidra/ExportFieldStoreCandidates.java` performs a bounded High P-code scan of functions whose names contain one of the requested class tokens. For every `STORE`, it recursively follows the destination-address dependency chain and emits a candidate only when the chain contains an exact `PTRSUB` or `INT_ADD` with the requested constant field offset.

The wrapper invokes the equivalent of:

```text
ExportFieldStoreCandidates.java "EnemyActionsSystem;MeActionsSystem" 0x294 /tmp/player-current-frame-stores.tsv
```

The TSV contains derived metadata only:

- containing function entry/name;
- STORE P-code address/time;
- requested field offset;
- matching address-expression opcode and P-code location;
- base/value dependency tokens;
- whether the stored value is a constant and, when it is, that constant value.

It does not export instruction bytes, assembly/disassembly text, decompiler C text, original strings, resources, or assets.

## Validation

The headless wrapper runs the same validator automatically. It can also be invoked directly:

```bash
python3 tools/ghidra/summarize_field_store_candidates.py \
  /tmp/player-current-frame-stores.tsv --field-offset 0x294
```

The validator fails closed on schema errors, wrong field offsets, unsupported address opcodes, contradictory constant metadata and duplicate STORE identities. The summary separates constant stores (especially literal zero) from values derived through other P-code operations.

A constant-zero candidate is **not by itself sufficient** to claim the initial player frame is zero. The candidate must still be associated with the construction/initial-action path for `MeActionsSystem::creinitWithFile` or an immediately following proven transition. This avoids confusing later waUpdate reset behavior with first-spawn initialization.

## Acceptance for the next runtime step

The frame blocker is closed only when derived evidence establishes all of the following:

1. a write/transition that determines `+0x294` for the initial `action28` `MeActionsSystem` instance;
2. the value written or the deterministic source of that value;
3. ordering relative to `loadingTex()`/the point where `+0x26c` becomes ready;
4. no ambiguity with unrelated later action-update/reset paths.

Once these are established, the existing `player_action_document_store`, `game_scene_player_spawn_state`, and `game_scene_player_frame_request` can be composed into the first visible project-owned player sprite without guessed frame selection.
