# ProductionClassifier/3 OOD family-similarity gate

`ProductionClassifier/2` uses predicted-class-specific confidence calibration.
A 500-function export from `libcocos2dcpp.so` that had zero training-identity
overlap exposed a remaining problem: `media_library` received many automatic
accepts because the calibration corpus for that class is dominated by FFmpeg.
That is a source-domain shift problem, not a basic classifier-accuracy problem.

Version 3 keeps the same semantic model and adds a structural novelty gate:

```text
portable P-code
      |
      +--> TF-IDF + logistic regression -> class + confidence
      |
      +--> TF-IDF family prototypes -> nearest-family cosine similarity
                                      |
classwise confidence accept ----------+----> accept only if BOTH pass
```

The gate does not use binary names, source function names or addresses.

## Train

```bash
cd ~/nevergone-recomp
source /tmp/nevergone-ml-venv/bin/activate

rm -rf /tmp/nevergone-ml-v2/production-code-domain-v3

python3 tools/ml/train_nevergone_production_classifier_v3.py \
  /tmp/nevergone-ml-v2/dataset-4class \
  /tmp/nevergone-ml-v2/families.jsonl \
  /tmp/nevergone-ml-v2/production-code-domain-v3 \
  --classes game engine runtime_library media_library
```

For every family-grouped OOF fold, the trainer builds family centroids only from
the fold's training functions. Each held-out function is compared with training
family prototypes of its predicted class. The final `ood_accept_threshold` is
therefore estimated without comparing a function with its own family.

## Inference

The normal inference CLI reads v1, v2 and v3 artifacts:

```bash
python3 tools/ml/classify_nevergone_pcode.py \
  /tmp/nevergone-ml-v2/production-code-domain-v3 \
  /tmp/nevergone-new-functions.jsonl \
  --output /tmp/nevergone-new-functions-v3.classified.jsonl
```

A v3 result additionally contains:

```json
{
  "family_similarity": 0.63,
  "nearest_training_family": "family:...",
  "ood_accept_threshold": 0.58,
  "ood_accept_pass": true
}
```

A prediction that passes classwise confidence but fails the OOD threshold is
demoted from `accept` to `review` (or `escalate` if no review tier is enabled).
If a class has too few safe OOF examples to derive an OOD threshold, automatic
acceptance for that class is disabled.

## Interpretation

The gate is deliberately conservative. Its purpose is to stop high-confidence
predictions from being treated as automatically trustworthy when their portable
P-code is unlike the known function families used to calibrate that class.

It does not prove semantic correctness and cannot fully solve source confounding
when an out-of-domain function is structurally very similar to an in-domain
family. A true cross-binary validation set remains the strongest next test.
