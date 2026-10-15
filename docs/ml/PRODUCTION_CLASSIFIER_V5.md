# Production Classifier v5: objective evidence fusion

`ProductionClassifier/5` keeps the validated sparse portable-P-code baseline and
adds label-free structural evidence. It is deliberately not a larger neural model.

The current local 9,118-function reference run established the comparison point:

```text
ProductionClassifier/4:
  balanced accuracy  0.8160
  macro F1           0.7370
  accept precision   0.9661
  accept coverage    0.6023

Hybrid residual:
  balanced accuracy  0.8151
  macro F1           0.7447
```

The standalone tiny encoder remained materially below the sparse baseline, so v5
tests whether deterministic analysis evidence improves the existing classifier
instead of increasing neural capacity.

## Representation

```text
portable P-code
     |
     +--> TF-IDF unigram/bigram
     |
     +--> bucketed structural metrics derived from portable P-code
     |
     +--> optional callgraph evidence
     |
     +--> optional receiver/type + receiver/vtable summaries
     |
     v
balanced logistic regression
     |
classwise calibration
     |
contrastive family OOD gate
     |
accept / review / escalate
```

Evidence is represented as coarse sparse tokens such as count/range buckets.
Exact receiver class names, vtable identities, source addresses and project labels
are excluded.

The receiver aggregation currently uses only objective facts such as:

- receiver call/presence counts;
- pointer-depth and storage-kind counts;
- unique/ambiguous transparent type-flow counts;
- inference status counts;
- resolved/runtime-matched receiver counts;
- class-evidence source kinds;
- coarse dispatch-slot buckets.

It intentionally does **not** emit the recovered class name itself. This keeps the
experiment useful for stripped/cross-binary generalization instead of turning a
Ghidra symbol name into a hidden class label.

## Build evidence and train

The persistent local ML root used by the current Nevergone workflow is:

```text
~/.local/share/nevergone-ml-v2
```

With the v2 dataset/corpus already built:

```bash
cd ~/nevergone-recomp
source ~/.venvs/nevergone-ml/bin/activate

bash tools/ml/run_nevergone_production_classifier_v5.sh \
  ~/.local/share/nevergone-ml-v2
```

The runner always extracts callgraph evidence from the checked-in full indexes.

If receiver evidence exists under:

```text
~/.cache/nevergone-recomp/receiver-provenance/
```

the runner also consumes `indirect-receiver-types.tsv` and chooses the newest
available `static-receiver-vtable-inference-vN.jsonl` from v6 down to v2.
Receiver evidence is optional; structural evidence is always derived directly
from portable P-code.

Outputs:

```text
~/.local/share/nevergone-ml-v2/evidence-v5/evidence.jsonl
~/.local/share/nevergone-ml-v2/models/production-code-domain-v5/classifier.joblib
~/.local/share/nevergone-ml-v2/models/production-code-domain-v5/manifest.json
~/.local/share/nevergone-ml-v2/models/production-code-domain-v5/oof_predictions.jsonl
~/.local/share/nevergone-ml-v2/logs/v5-evidence.log
~/.local/share/nevergone-ml-v2/logs/v5-train.log
```

The runner prints a direct v4/v5 comparison when the v4 manifest is present.

## Manual commands

External evidence can be built independently:

```bash
python3 tools/ml/build_nevergone_v5_evidence.py \
  ~/.local/share/nevergone-ml-v2/dataset-4class/trace.jsonl \
  --callgraph-metrics ~/.local/share/nevergone-ml-v2/evidence-v5/libcocos2dcpp.callgraph.jsonl \
  --callgraph-metrics ~/.local/share/nevergone-ml-v2/evidence-v5/libffmpeg.callgraph.jsonl \
  --receiver-types ~/.cache/nevergone-recomp/receiver-provenance/indirect-receiver-types.tsv \
  --receiver-inference ~/.cache/nevergone-recomp/receiver-provenance/static-receiver-vtable-inference-v6.jsonl \
  --output ~/.local/share/nevergone-ml-v2/evidence-v5/evidence.jsonl
```

Then train:

```bash
python3 tools/ml/train_nevergone_production_classifier_v5.py \
  ~/.local/share/nevergone-ml-v2/dataset-4class \
  ~/.local/share/nevergone-ml-v2/families.jsonl \
  ~/.local/share/nevergone-ml-v2/models/production-code-domain-v5 \
  --evidence ~/.local/share/nevergone-ml-v2/evidence-v5/evidence.jsonl \
  --classes game engine runtime_library media_library
```

## Inference

`classify_nevergone_pcode.py` remains backward compatible with v1-v4 and now
accepts v5 artifacts.

Structural evidence is derived automatically from every input row. Optional
external evidence can be supplied with `--evidence`:

```bash
python3 tools/ml/classify_nevergone_pcode.py \
  ~/.local/share/nevergone-ml-v2/models/production-code-domain-v5 \
  /path/to/new-corpus.jsonl \
  --evidence /path/to/new-corpus-evidence.jsonl \
  --output /path/to/new-corpus-v5.classified.jsonl
```

Without `--evidence`, v5 still runs with portable P-code plus its derived
structural metrics. For a fair production comparison with a model trained with
receiver/callgraph evidence, provide the same evidence categories at inference.

## Evaluation rule

v5 must be compared against v4 on the same family-grouped folds and seed. The
primary targets are:

```text
balanced accuracy     > 0.8160
macro F1              > 0.7370, target > 0.7600
accept precision      >= 0.97
accept coverage       >= 0.60
game precision        materially above the v4 0.3164 prediction precision
```

A gain that appears only when raw class names or addresses are present is not
accepted. If evidence fusion does not improve family-held-out metrics, v4 remains
the production classifier and the new evidence is kept diagnostic-only.
