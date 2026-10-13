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

## 1. Export normalized high P-code

`tools/ghidra/ExportMlPcodeCorpus.java` is a bulk version of the existing focused
P-code flow exporter. It writes JSONL locally and intentionally excludes from
`model_text`:

- absolute function/instruction addresses;
- function/symbol names;
- assembly bytes or disassembly;
- decompiler C;
- strings and assets.

Small integer constants are retained because offsets, flags and loop bounds are
structural evidence. Large constants are bucketed so code/data addresses are not
accidental identifiers. Direct calls are represented only as internal, external
or unresolved; indirect calls are represented as indirect.

Start with a bounded smoke export:

```bash
"$GHIDRA_HOME/support/analyzeHeadless" \
  /path/to/project-dir project-name \
  -process '/program/path/libcocos2dcpp.so' \
  -noanalysis \
  -scriptPath "$PWD/tools/ghidra" \
  -postScript ExportMlPcodeCorpus.java \
    /tmp/nevergone-ml/libcocos2dcpp.sample.jsonl 250
```

The optional third script argument filters by function-name substring and is
useful for very small checks:

```text
-postScript ExportMlPcodeCorpus.java /tmp/sample.jsonl 50 EnemyActions
```

After the bounded export has been inspected, omit `max-functions` for the full
program. Export `libffmpeg.so` separately if it will be used as a labelled
media-library class, then concatenate the JSONL files locally.

Raw corpus JSONL is local analysis data and should not be committed.

## 2. Generate conservative seed labels

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

## 3. Add evidence-backed labels

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

## 4. Build deterministic splits

Concatenate the desired label files locally, then run:

```bash
python3 tools/ml/build_nevergone_dataset.py \
  /tmp/nevergone-ml/corpus.jsonl \
  /tmp/nevergone-ml/labels.jsonl \
  /tmp/nevergone-ml/dataset-v1
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
{"id":"...","text":"LANG=...\nCALLCONV=...\n...","labels":{"code_domain":"game"},"weights":{"code_domain":1.0}}
```

`trace.jsonl` keeps the local join back to program/address/name and evidence.
Training code should consume only the split JSONL `text`, `labels` and
`weights` fields.

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

## Important v1 limitations

- The split is deterministic by function identity, not yet grouped by structural
  function family. Near-duplicate helpers can therefore cross split boundaries.
  A future Function Family Miner should supply group IDs before model-quality
  claims rely on held-out accuracy.
- Exact strings are currently excluded from model input. Later experiments can
  add normalized string categories in a separate ablation instead of silently
  mixing them into the base representation.
- Register names are normalized away. This improves portability but may discard
  ABI information; calling convention and parameter count remain explicit.
- This is a representation/dataset experiment, not proof that a neural model can
  reconstruct exact source code.
