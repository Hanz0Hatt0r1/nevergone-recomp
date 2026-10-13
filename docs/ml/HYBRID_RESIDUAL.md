# Hybrid sparse + neural residual experiment

The first standalone `NevergoneTinyEncoder/1` neural baseline underperformed the
portable-P-code TF-IDF baseline on the four-class family-grouped experiment.
The observed local run was:

```text
TF-IDF family-grouped balanced accuracy: 0.8148 +/- 0.0384
Tiny encoder balanced accuracy:          0.7453 +/- 0.0658
Tiny encoder macro-F1:                  0.7296 +/- 0.0860
```

The failure mode was not uniform: one outer fold fell to 0.6417 balanced
accuracy, and `engine` recall was only 0.4553 overall. With 1,098 labelled
functions, a 1.58M-parameter standalone encoder is too data-hungry to justify
replacing the sparse linear baseline.

`tools/ml/nevergone_hybrid_residual_cv.py` therefore keeps the strongest known
portable-P-code classifier as a frozen base and trains a much smaller neural
residual only when nested family-grouped validation says it helps.

## Architecture

```text
portable P-code
      |\
      | +--> TF-IDF unigram/bigram -> logistic regression -> base logits
      |
      +----> hashed unigram/bigram -> 32d embedding -> 32d tanh -> residual logits
                                                        |
base logits + residual logits ---------------------------+
      |
      +--> code_domain
```

Default residual trainable parameter count is about 263k:

```text
8192 hash buckets x 32 embedding dimensions
32 x 32 hidden projection
32 x 4 output projection
biases
```

The TF-IDF linear model is fitted independently inside every inner/outer fold.
The residual path never sees source names, addresses, raw corpus headers or
project-relative labels.

## Epoch-zero fallback

Inner validation begins with the pure TF-IDF prediction as epoch 0. Neural
training must improve inner macro-F1 by `--min-delta` before a positive epoch is
selected. If it does not, `best_epoch=0` and the outer fold is evaluated with
the exact linear baseline.

This makes the experiment answer a narrower question than the standalone tiny
encoder:

> can a compact learned representation add useful nonlinear corrections to the
> already-strong sparse representation without sacrificing its baseline?

## Run

```bash
source /tmp/nevergone-ml-venv/bin/activate

python3 tools/ml/nevergone_hybrid_residual_cv.py \
  /tmp/nevergone-ml-v2/dataset-4class \
  /tmp/nevergone-ml-v2/families.jsonl \
  --classes game engine runtime_library media_library
```

The evaluator prints both base and hybrid metrics for every outer fold:

```text
fold=1 base_balanced=... hybrid_balanced=... epochs=...
```

and aggregate values:

```text
base balanced: ...
hybrid balanced: ...
base macro_f1: ...
hybrid macro_f1: ...
```

Interpretation:

- hybrid clearly above base: keep the residual encoder and test multi-task heads;
- hybrid approximately equal to base: the dataset is not yet large enough for a
  useful learned representation, so keep TF-IDF as the production router;
- many folds select epoch 0: collect substantially more independent families
  before increasing neural capacity;
- engine remains the dominant confusion: focus new evidence/data collection on
  game-vs-engine boundary functions rather than adding more easy FFmpeg/runtime
  examples.
