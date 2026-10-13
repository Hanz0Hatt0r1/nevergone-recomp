# Production portable-P-code classifier

The validated Nevergone routing baseline is intentionally simple:

```text
Ghidra high P-code
      ↓
portable P-code
      ↓
TF-IDF unigram/bigram
      ↓
balanced logistic regression
      ↓
code_domain + calibrated confidence + action
```

On the four-class, family-grouped experiment the sparse baseline reached about
`0.818` balanced accuracy and outperformed both the standalone tiny encoder and
the hybrid neural-residual experiment. This document turns that baseline into a
reusable local artifact instead of continuing architecture search.

The production path still excludes source function names, source addresses,
raw Ghidra headers and project-relative goals from model input.

## Train and package

Use the already-built four-class dataset and Function Family Miner map:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
source /tmp/nevergone-ml-venv/bin/activate

rm -rf /tmp/nevergone-ml-v2/production-code-domain

python3 tools/ml/train_nevergone_production_classifier.py \
  /tmp/nevergone-ml-v2/dataset-4class \
  /tmp/nevergone-ml-v2/families.jsonl \
  /tmp/nevergone-ml-v2/production-code-domain \
  --classes game engine runtime_library media_library
```

The output directory contains:

```text
classifier.joblib
manifest.json
oof_predictions.jsonl
```

`classifier.joblib` contains the final TF-IDF/logistic model trained on all
selected labelled functions plus the confidence calibrator and routing
thresholds. It is local derived analysis data and should not be committed.

`manifest.json` records the dataset fingerprint, classes, OOF metrics,
calibration policy and selected thresholds.

`oof_predictions.jsonl` is the audit trail used to understand routing quality.
Each row contains truth, prediction, raw score, calibrated confidence and
routing action for a function that was held out together with its function
family.

## Confidence calibration

The final classifier is trained on every selected labelled function, but its
routing thresholds are **not** estimated from training-set predictions.

The trainer first generates family-grouped out-of-fold predictions. The maximum
logistic probability is then calibrated against the binary event
`prediction == truth` with isotonic regression.

The resulting `confidence` means:

> estimated probability that the predicted class is correct, calibrated from
> family-held-out Nevergone functions.

The default routing policy searches for the maximum OOF coverage that satisfies:

```text
accept:   empirical precision >= 0.95
review:   empirical precision >= 0.85
escalate: everything below the review threshold
```

At least 25 OOF examples must pass a threshold before it is accepted. If no
threshold has sufficient evidence, that action tier is disabled rather than
inventing a cutoff.

Targets can be changed explicitly:

```bash
python3 tools/ml/train_nevergone_production_classifier.py \
  /tmp/nevergone-ml-v2/dataset-4class \
  /tmp/nevergone-ml-v2/families.jsonl \
  /tmp/nevergone-ml-v2/production-code-domain \
  --classes game engine runtime_library media_library \
  --accept-precision 0.97 \
  --review-precision 0.90
```

## Classify a new Ghidra export

The inference CLI accepts JSONL written directly by `ExportMlPcodeCorpus.java`:

```bash
python3 tools/ml/classify_nevergone_pcode.py \
  /tmp/nevergone-ml-v2/production-code-domain \
  /tmp/new-functions.jsonl \
  --output /tmp/new-functions.classified.jsonl
```

It also accepts dataset-style rows containing `text` instead of `model_text`.
Use `-` as input/output for streaming:

```bash
cat /tmp/new-functions.jsonl | \
python3 tools/ml/classify_nevergone_pcode.py \
  /tmp/nevergone-ml-v2/production-code-domain \
  -
```

Typical output:

```json
{"action":"accept","confidence":0.972,"id":"libcocos2dcpp.so:00123456","prediction":"game","probabilities":{"engine":0.021,"game":0.961,"media_library":0.006,"runtime_library":0.012},"raw_confidence":0.961,"task":"code_domain"}
```

Ambiguous functions are explicitly surfaced instead of forced into automation:

```json
{"action":"escalate","confidence":0.58,"id":"libcocos2dcpp.so:00124567","prediction":"engine","raw_confidence":0.54,"task":"code_domain"}
```

For a compact stream, omit the full probability map:

```bash
python3 tools/ml/classify_nevergone_pcode.py \
  /tmp/nevergone-ml-v2/production-code-domain \
  /tmp/new-functions.jsonl \
  --no-probabilities
```

## Intended automation

The classifier is a cheap first-stage router, not a source-code reconstructor:

```text
new binary/function
      ↓
Ghidra export
      ↓
production classifier
      ↓
 ┌─────────────┬─────────────┬─────────────┐
 │ accept      │ review      │ escalate    │
 │ high conf   │ medium conf │ ambiguous   │
 ↓             ↓             ↓
automation   evidence UI   deeper analysis / LLM
```

The next useful model heads are `subsystem` and later `function_role`. They
should reuse this confidence-routing machinery rather than adding larger neural
models before there are substantially more independent function families.
