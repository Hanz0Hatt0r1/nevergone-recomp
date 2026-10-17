# CCJoyStick::init field store/load CFG audit

This records a read-only generic `revenginstr` audit for the Nevergone
`libcocos2dcpp.so` function `CCJoyStick::init` at Ghidra entry `002a6584`.
The source binary is identified by SHA-256; the binary itself is not included.

## Run identity

- Ghidra project/program: `nevergone` / `libcocos2dcpp.so`
- Ghidra version: `12.1.2_DEV`
- Ghidra mode: `-readOnly -noanalysis`
- Source binary: `armeabi-v7a/libcocos2dcpp.so`
- Source binary SHA-256: `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e`
- Generic tooling: `Hanz0Hatt0r1/revenginstr` main at `659a6c5` (PRs [#89](https://github.com/Hanz0Hatt0r1/revenginstr/pull/89) and [#90](https://github.com/Hanz0Hatt0r1/revenginstr/pull/90))
- Opaque join key: `nevergone:libcocos2dcpp.so`

The exporters opened the existing analyzed project read-only and disabled
analysis. The raw evidence here contains function addresses and P-code-derived
rows, so it remains in this consumer repository.

## Result

The shared parameter-field exporter emitted 6 parameter-rooted stores and 9
parameter-rooted loads. Exact program/function/formal-parameter/offset pairing
produced 2 graphs. The shared CFG path audit produced:

| Field offset | Store (address, time) | Load (address, time) | Status | Path witness | First explicit barrier before the load |
|---|---|---|---|---|---|
| `0x119` | `002a65a8`, 49 | `002a6648`, 168 | `intervening_store_or_call` | `[2]` | `STORE` at `002a65b0`, time 55 |
| `0x11a` | `002a65b8`, 60 | `002a6658`, 209 | `intervening_store_or_call` | `[2, 3]` | `STORE` at `002a65c0`, time 65 |

No pair had a path compatible with the audit's current barrier policy. The
barrier is an explicit P-code store, but the exporter does not establish
whether that store aliases either paired field. This is a negative CFG-path
compatibility result; it does not prove that a store fails to reach a load, or
that either access has a particular runtime meaning.

## Reproduction

Run from a checkout with the shared Ghidra scripts and `revenginstr` CLI:

```bash
GHIDRA_HOME=/opt/ghidra scripts/run_ghidra_parameter_store_field.sh \
  /path/to/projects nevergone libcocos2dcpp.so 002a6584 parameter-stores.tsv \
  --load-output parameter-loads.tsv

GHIDRA_HOME=/opt/ghidra scripts/run_ghidra_function_pcode_cfg.sh \
  /path/to/projects nevergone libcocos2dcpp.so function-pcode-cfg.jsonl \
  'nevergone:libcocos2dcpp.so' 002a6584

revenginstr-receiver-field-access-pairs parameter-stores.tsv \
  parameter-loads.tsv --program-id 'nevergone:libcocos2dcpp.so' \
  --output field-access-pairs.jsonl

revenginstr-receiver-field-store-load-audit field-access-pairs.jsonl \
  function-pcode-cfg.jsonl --output field-store-load-path-audit.jsonl
```

## Artifact hashes

| Artifact | SHA-256 |
|---|---|
| Source binary (not included) | `94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e` |
| `parameter-stores.tsv` | `308a90c88b719f1bb0bdbd3f605dd18968a2fe4e284b51cde6ac2a4c63749bc0` |
| `parameter-loads.tsv` | `b96a464135852bc16474bb3e9e6d127b07527330a8e128b67b4907be999df134` |
| `field-access-pairs.jsonl` | `a914bacb54e8b0996091e443184ac456bb5c3ce93bc400759764a66e7d604e43` |
| `function-pcode-cfg.jsonl` | `30e473fdf66e933f354ddcb78fdecc92355f7d59feb95b45ea59318d334b6e53` |
| `field-store-load-path-audit.jsonl` | `682fdc63b2b908e54515323d2fd7fd496f5b38e4f586c84321e17e38f0fd8e31` |

The audit rows also pin the pair-graph and CFG hashes and retain the original
store/load source identities.

## Frozen workflow replay

[`benchmark-manifest.json`](benchmark-manifest.json) pins the pair graphs, the
existing CFG/1 rows, four schema snapshots from `revenginstr` main at
`8288e07`, and the expected path-audit output. It replays the original path
audit and the new conservative dominance audit, with exact-evidence gates for
both outputs.

Run it with that `revenginstr` revision available locally:

```bash
PYTHONPATH=/path/to/revenginstr/src \
  python -m revenginstr.benchmark \
  docs/evidence/field-store-load-cfg-ccjoystick-init/benchmark-manifest.json \
  --mode smoke --output /path/to/benchmark-summary.json
```

The benchmark refuses to overwrite declared outputs. For another replay,
remove the local `frozen-replay/` outputs or use a fresh checkout.

The frozen replay passed both stages and both zero-mismatch exact gates. The
dominance output has `incomplete_evidence` for both offsets because the pinned
CFG/1 artifact has no explicit entry block, memory space, or memory width. This
does not mean the paired stores fail to reach their loads. A fresh CFG/2 export
is required before the dominance audit can make a structural candidate; use
`--memory-metadata` on the shared CFG exporter. The generated `frozen-replay/`
files are outputs and are intentionally not checked in; the expected rows and
all input/schema hashes are pinned in this consumer-owned manifest.

The expected dominance rows are in
[`field-store-load-dominance-audit.expected.jsonl`](field-store-load-dominance-audit.expected.jsonl)
with SHA-256 `d2817879c2efce3d701a43190cdcf65613dcfc6ca8158fa9fcad92b87c26e033`.
The frozen manifest SHA-256 is `c8ad5401f1a582520c2b3706cda0f4d19d39111c8e76702b14a1046116ddc5c6`.
They preserve the missing-metadata result instead of inferring aliasing or
memory value flow from CFG reachability.
