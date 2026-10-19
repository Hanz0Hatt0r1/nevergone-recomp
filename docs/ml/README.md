# Nevergone ML testbed

This directory documents the first project-local ML corpus pipeline. Nevergone is
used as the testbed before the same representation is generalized to larger
recompilation projects.

The first target is deliberately narrow: learn objective code properties from
normalized high P-code without teaching the model addresses, `FUN_*` identities,
or a mutable project priority.

## Existing evidence reused

The repository already contains:

- `docs/ghidra/full-index/libcocos2dcpp.*.tsv.gz` and
  `libffmpeg.*.tsv.gz` for functions, symbols, strings, xrefs and calls;
- focused high-P-code and operand exporters under `tools/ghidra/`;
- bounded unidbg/native evidence for HPData, GameSaveData, EnemyActionsData and
  related code paths;
- many source-backed behavior/layout notes under `docs/evidence/`.

The ML pipeline does not replace those evidence layers. It converts selected,
auditable evidence into labels and uses a normalized code representation as
model input.

## 1. Export canonical high P-code

New exports use the shared `revenginstr` Ghidra runner and write
`REVENGINSTR.PcodeCorpus/1`. The Nevergone dataset builder and frozen-corpus
selector also accept retained `NEVERGONE.MlPcodeCorpus/2` exports. When the
shared checkout is available, `run_nevergone_frozen_unseen500_v10.sh` uses it;
the local Java exporter remains as an offline compatibility fallback.

`model_text` intentionally excludes:

- absolute function/instruction addresses;
- function/symbol names;
- assembly bytes or disassembly;
- decompiler C;
- strings and assets;
- raw Ghidra language variants such as `ARM:LE:32:v8`;
- Ghidra address-space IDs used as the first LOAD/STORE operand;
- direct branch/return target bookkeeping;
- the second internal iop reference used by high-P-code `INDIRECT`.

Small integer constants are retained because object-field offsets, flags and
loop bounds are structural evidence. Large constants are bucketed so code/data
addresses do not become accidental identifiers. Direct calls are represented
only as internal, external or unresolved; indirect calls are represented as
indirect.

The architecture header is canonicalized. For example, a Ghidra language ID
such as `ARM:LE:32:v8` becomes:

```text
ARCH=ARM32
ENDIAN=LE
PTR_SIZE=4
```

Exact function-size and operation counters are kept only in the outer audit
`metrics` object. `model_text` receives coarse count buckets instead, reducing
single-game fingerprinting while preserving useful scale information.

Model input is capped at 256 P-code operations. Functions above the cap use a
deterministic head+tail representation (128 operations from each end) and set
`TRUNCATED=1`. References from an emitted operation to a definition in the
omitted middle are represented as `def:omitted:<size>`.

Start with a bounded smoke export from an already analyzed project:

```bash
bash "$REVENGINSTR_REPO/scripts/run_ghidra_pcode_export.sh" \
  /path/to/project-dir project-name libcocos2dcpp.so \
  /tmp/nevergone-ml/libcocos2dcpp.sample.jsonl 250
```

The optional final runner argument filters by function-name substring and is
useful for focused checks. The retained local exporter can still read older
Nevergone command lines when the shared checkout is unavailable:

```text
-postScript ExportMlPcodeCorpus.java /tmp/sample.jsonl 50 EnemyActions
```

A first real EnemyActions run successfully exported 50 functions and exposed
three decompiler-internal features that were too noisy for training: LOAD/STORE
address-space IDs, control-flow target operands and INDIRECT iop references.
Those are removed by corpus format v2.

Old `NEVERGONE.MlPcodeCorpus/1` files are intentionally rejected. Re-export them
with the shared runner or the retained compatibility exporter instead of mixing
representations inside one experiment. Dataset manifests record the source
corpus format; the emitted Nevergone dataset format and labels remain unchanged.

Raw corpus JSONL is local analysis data and should not be committed.

## 2. Inspect the real corpus

Validate the v2 representation and get size/complexity statistics before
training:

```bash
python3 tools/ml/inspect_nevergone_corpus.py \
  /tmp/nevergone-ml/libcocos2dcpp.sample.jsonl
```

The report includes:

- successful/failed decompilations;
- number of truncated functions;
- architecture and calling-convention distributions;
- median, p95 and maximum original P-code operation counts;
- emitted operation counts after the 256-op cap;
- model-text byte sizes.

Use `--json` for machine-readable output.

## 3. Generate conservative seed labels

The checked-in full Ghidra function index can provide direct-name supervision
without exposing those names to the model.

```bash
python3 tools/ml/build_nevergone_seed_labels.py \
  docs/ghidra/full-index/libcocos2dcpp.functions.tsv.gz \
  libcocos2dcpp.so \
  tools/ml/nevergone_seed_rules.json \
  /tmp/nevergone-ml/libcocos2dcpp.labels.jsonl
```

Run the same command against `libffmpeg.functions.tsv.gz` with program
`libffmpeg.so` when that corpus has also been exported.

The initial rule file is intentionally small. It labels only direct identities
such as known Nevergone classes, the `cocos2d::` namespace, standard-library
namespaces and the separately indexed `libffmpeg.so` program. Expand it only
when the evidence boundary is explicit.

## 4. Add evidence-backed labels

Additional labels use one JSON object per task:

```json
{"program":"libcocos2dcpp.so","source_name":"EnemyActionsData::loadWBGFile(char const*)","task":"function_role","label":"asset_parser","quality":"gold","evidence":"docs/evidence/enemy-actions-wbg-document.md"}
```

A label may target `source_entry` instead of `source_name`. If a name is not
unique in the corpus, the dataset builder fails rather than guessing.

Quality defaults:

| quality | training weight | intended meaning |
| --- | ---: | --- |
| `gold` | 1.0 | direct symbol/runtime/independent proof |
| `silver` | 0.6 | strong but incomplete cross-checked evidence |
| `bronze` | 0.3 | useful candidate, not semantic proof |

An explicit numeric `weight` in `(0, 1]` can override the default.

Do **not** label an unclassified function as `other` merely because the project
has not investigated it yet.

## 5. Build deterministic splits

Concatenate the desired label files locally, then run:

```bash
python3 tools/ml/build_nevergone_dataset.py \
  /tmp/nevergone-ml/corpus.jsonl \
  /tmp/nevergone-ml/labels.jsonl \
  /tmp/nevergone-ml/dataset-v2
```

Outputs:

```text
train.jsonl
validation.jsonl
test.jsonl
trace.jsonl
manifest.json
```

Training rows contain only:

```json
{"id":"...","text":"ARCH=ARM32\nENDIAN=LE\n...","labels":{"code_domain":"game"},"weights":{"code_domain":1.0}}
```

`trace.jsonl` keeps the local join back to program/address/name, exact metrics
and evidence. Training code should consume only the split JSONL `text`,
`labels` and `weights` fields.

The v2 splitter hashes canonical `model_text` to form an exact structural group.
Byte-identical normalized functions therefore always land in the same split.
This blocks the easiest duplicate-function leakage. Near-duplicate family
grouping still requires the planned Function Family Miner.

## First experiment

Do not start with a broad semantic taxonomy. The first useful baseline is:

```text
code_domain:
  game
  engine
  media_library
  runtime_library
```

Then, only where direct labels are numerous enough:

```text
subsystem:
  scene
  combat
  assets_data
  save
  ...
```

Function-role heads (`parser`, `constructor`, `update`, `loader`, etc.) should be
added separately instead of forcing all semantics into one class.

## Important v2 limitations

- Exact structural duplicates are grouped for splitting, but near-duplicate
  helpers are not yet clustered. A future Function Family Miner should provide
  family IDs before held-out accuracy is treated as cross-family generalization.
- Large functions use head+tail truncation. A later experiment can compare this
  against multi-chunk embeddings without changing the evidence labels.
- Exact strings are excluded from model input. Later experiments can add
  normalized string categories in a separate ablation instead of silently
  mixing them into the base representation.
- Register names are normalized away. This improves portability but may discard
  ABI information; calling convention and parameter count remain explicit.
- This is a representation/dataset experiment, not proof that a neural model can
  reconstruct exact source code.

## Manual-recovery candidate retrieval

The labelled corpus can also prioritize unknown functions for human recovery.
Use task-filtered family similarity, exclude query families that already have
labels for that task, and keep the result advisory. The current manual labels
have many one-off function roles, so nearest labelled examples are more useful
for triage than an unvalidated role classifier. See
[`MANUAL_RECOVERY_RETRIEVAL.md`](MANUAL_RECOVERY_RETRIEVAL.md) for the current
v70 build and query commands and retrieval results.
