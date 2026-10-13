# Production classifier OOD gates

`ProductionClassifier/2` uses predicted-class-specific confidence calibration.
A 500-function export from `libcocos2dcpp.so` with zero training-identity overlap
showed that confidence alone could still over-accept `media_library`, whose
labelled examples are dominated by FFmpeg.

## Version 3: absolute family similarity

Version 3 kept the semantic classifier unchanged and required a second signal:
cosine similarity to a known training-family prototype of the predicted class.
The held-out 500-function test showed that this absolute gate was still too weak:
40 `media_library` functions remained accepted because most were structurally
similar to some FFmpeg family and the learned media threshold was only about
0.38.

## Version 4: contrastive family similarity

Version 4 adds a stronger question:

> Is the function more similar to a known family of the predicted class than to
> the strongest family from any competing class?

For every function it records:

```text
predicted_similarity = nearest prototype from predicted class
competing_similarity = nearest prototype from every other class
contrast_margin      = predicted_similarity - competing_similarity
```

Automatic acceptance requires all three conditions:

```text
classwise confidence >= class accept threshold
predicted_similarity >= class OOD similarity threshold
contrast_margin      >= class contrast threshold
```

The contrast threshold is estimated from family-grouped OOF examples, but has a
hard production floor of `0.0`. Therefore a function can never be automatically
accepted when another class has stronger family-level structural evidence.

This is specifically intended to suppress generic wrappers/helpers that happen
to resemble an FFmpeg family but resemble engine/game/runtime families just as
much or more.

The gate still does **not** use binary names, source function names, addresses,
strings, or a current project goal.

## Train v4

```bash
cd ~/nevergone-recomp
source /tmp/nevergone-ml-venv/bin/activate

rm -rf /tmp/nevergone-ml-v2/production-code-domain-v4

python3 tools/ml/train_nevergone_production_classifier_v4.py \
  /tmp/nevergone-ml-v2/dataset-4class \
  /tmp/nevergone-ml-v2/families.jsonl \
  /tmp/nevergone-ml-v2/production-code-domain-v4 \
  --classes game engine runtime_library media_library
```

For every family-grouped OOF fold, prototypes are built only from training
families. Held-out functions are compared against predicted-class and competing
class prototypes, so neither the absolute similarity nor the contrastive margin
compares a function with its own held-out family.

## Inference

The normal inference CLI is backward compatible with v1-v4 artifacts:

```bash
python3 tools/ml/classify_nevergone_pcode.py \
  /tmp/nevergone-ml-v2/production-code-domain-v4 \
  /tmp/nevergone-new-functions.jsonl \
  --output /tmp/nevergone-new-functions-v4.classified.jsonl
```

A v4 result additionally contains:

```json
{
  "family_similarity": 0.63,
  "nearest_training_family": "family-...",
  "competing_similarity": 0.61,
  "nearest_competing_family": "family-...",
  "nearest_competing_label": "engine",
  "contrast_margin": 0.02,
  "contrast_accept_threshold": 0.05,
  "contrast_accept_pass": false
}
```

A high-confidence prediction that fails either structural gate is demoted from
`accept` to `review` (or to `escalate` when no review tier is available).

## Interpretation

The contrastive gate is still an OOD heuristic, not proof of semantics. Its
purpose is selective automation: accept only functions for which both the
classifier and the local family geometry point to the same class.

The strongest future validation remains a genuinely labelled cross-binary test
set. Until that exists, absolute and contrastive family evidence should be
reported explicitly rather than hidden behind a single confidence number.
