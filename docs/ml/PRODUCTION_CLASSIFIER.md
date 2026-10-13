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
the hybrid neural-residual experiment.

The production path excludes source function names, source addresses, raw Ghidra
headers and project-relative goals from model input.

## Why routing is class-conditional

The first production artifact used one global correctness calibrator and one set
of confidence thresholds. A held-out 500-function `libcocos2dcpp.so` export was
100% unseen by training identity, but its automatic decisions were strongly
skewed:

```text
accept:   143 / 500
review:   349 / 500
escalate:   8 / 500

accepted classes:
engine         140
media_library    3
game             0
```

A global threshold can therefore be dominated by easier classes. Production
Classifier v2 instead conditions both confidence calibration and routing policy
on the **predicted class**.

For each predicted class, family-grouped OOF predictions are used to:

1. fit an isotonic correctness calibrator when enough varied evidence exists;
2. find the widest `accept` region reaching the target empirical precision;
3. find the widest `review` region reaching the lower target precision.

If a class has insufficient evidence for a threshold, that tier is disabled for
that class. The router escalates rather than borrowing an unsafe global cutoff.
A global OOF isotonic calibrator is retained only as a confidence-calibration
fallback when a class cannot fit its own mapping.

## Train and package

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

`classifier.joblib` contains the final model, global fallback calibrator,
class-specific calibrators and class-specific routing policies.

`manifest.json` records the dataset fingerprint, OOF metrics, calibrator sources,
per-class policies and selective-risk statistics.

`oof_predictions.jsonl` is the family-held-out audit trail. Each row includes the
predicted class, correctness, raw score, calibrated confidence,
`confidence_source`, selected thresholds and routing action.

## Default safety targets

The defaults are:

```text
accept: empirical OOF precision >= 0.95 within predicted class
review: empirical OOF precision >= 0.85 within predicted class
```

At least 10 predicted-class OOF examples must pass a class threshold. At least 25
predicted-class examples with both correct and incorrect outcomes are required
to fit a class-specific isotonic calibrator. These can be changed explicitly:

```bash
python3 tools/ml/train_nevergone_production_classifier.py \
  /tmp/nevergone-ml-v2/dataset-4class \
  /tmp/nevergone-ml-v2/families.jsonl \
  /tmp/nevergone-ml-v2/production-code-domain \
  --classes game engine runtime_library media_library \
  --accept-precision 0.97 \
  --review-precision 0.90 \
  --min-class-threshold-examples 15
```

Training prints entries such as:

```text
class policies:
  game: predicted=... precision=... calibrator=classwise accept=... review=...
  engine: predicted=... precision=... calibrator=classwise accept=... review=...
...
OOF actions: ...
selective OOF: ...
```

`selective OOF` reports the actual held-out precision and coverage of the
`accept`, `review` and `escalate` buckets after combining all class policies.

## Classify a new Ghidra export

```bash
python3 tools/ml/classify_nevergone_pcode.py \
  /tmp/nevergone-ml-v2/production-code-domain \
  /tmp/nevergone-new-functions.jsonl \
  --output /tmp/nevergone-new-functions.classified.jsonl
```

Typical v2 result:

```json
{"action":"accept","accept_threshold":0.91,"confidence":0.97,"confidence_source":"classwise","id":"libcocos2dcpp.so:00123456","prediction":"engine","raw_confidence":0.94,"review_threshold":0.77,"task":"code_domain"}
```

The output exposes the policy actually used for that predicted class. This is
particularly useful when auditing surprising `media_library` or `game`
predictions in a new binary.

The inference CLI remains backward compatible with
`NEVERGONE.ProductionClassifier/1` artifacts, but v1 keeps its legacy global
routing. Retrain to obtain v2 behavior.

For a compact stream, omit the full probability map:

```bash
python3 tools/ml/classify_nevergone_pcode.py \
  /tmp/nevergone-ml-v2/production-code-domain \
  /tmp/nevergone-new-functions.jsonl \
  --no-probabilities
```

## Intended automation

```text
new binary/function
      ↓
Ghidra export
      ↓
portable production classifier
      ↓
 predicted class
      ↓
class-specific calibration + threshold
      ↓
 ┌─────────────┬─────────────┬─────────────┐
 │ accept      │ review      │ escalate    │
 │ high safety │ medium      │ ambiguous   │
 ↓             ↓             ↓
automation   evidence UI   deeper analysis / LLM
```

After this routing layer is stable, the next useful model head is `subsystem`,
followed by `function_role`. Both should reuse the same family-grouped,
class-conditional confidence machinery.
