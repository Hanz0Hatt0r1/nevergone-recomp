# Nevergone tiny encoder

The first trainable neural baseline intentionally stays much smaller than a
language model. It is designed for the validated portable P-code representation
and for CPU use on modest developer hardware.

## Architecture

```text
canonical high P-code
  -> portable P-code
  -> deterministic hashed token unigrams + within-op bigrams
  -> learned mean embedding
  -> tanh hidden layer
  -> task classifier
```

Default dimensions:

```text
hash buckets:   16384
embedding dim:  96
hidden dim:     64
classes:        4
parameters:     1,579,332
```

The model input excludes source names, source addresses, raw Ghidra language
headers, calling-convention headers, exact SSA IDs, operation ordinals and exact
small offsets. The same `portable_pcode()` normalization used by the family
miner is applied before feature hashing.

This model is closer to a small fastText-style code encoder than a Transformer.
Local operation order is retained through adjacent token bigrams, while training
cost and memory use stay low.

## Why this experiment comes after family grouping

The portable TF-IDF baseline reached about 0.825 balanced accuracy on four
`code_domain` classes with exact structural grouping. After Function Family
Miner grouping it retained about 0.815 balanced accuracy. That small drop means
near-duplicate leakage does not explain most of the observed signal, so a small
learned representation is now worth testing.

The tiny encoder keeps the same family-grouped outer evaluation. Each outer fold
also gets a nested family-grouped validation split used only to choose the epoch
count. The model is then reinitialized and retrained on the complete outer
training partition for that number of epochs before the held family fold is
scored.

## Requirements

The trainer uses NumPy and scikit-learn only. No GPU framework is required.

```bash
python -m pip install -U numpy scikit-learn
```

## Four-class family-grouped CV

Assuming the v2 dataset and family map from the previous steps:

```bash
python3 tools/ml/nevergone_tiny_encoder_cv.py \
  /tmp/nevergone-ml-v2/dataset-4class \
  /tmp/nevergone-ml-v2/families.jsonl \
  --classes game engine runtime_library media_library
```

The default run uses five outer folds, nested five-fold epoch selection, up to
40 epochs and early stopping. Useful knobs for faster experimentation:

```bash
--epochs 25
--patience 5
--embedding-dim 64
--hidden-dim 48
--buckets 8192
```

Do not tune against the outer-fold results. Change hyperparameters only through
a separate experiment protocol or the nested validation scores.

## Evaluation target

The relevant comparison is the family-grouped portable TF-IDF baseline:

```text
balanced accuracy ~0.815
macro F1          ~0.812
```

The first neural experiment does not need to beat that result to be useful. It
also tests whether a compact learned embedding can recover portable structural
signal with a small dense representation suitable for later multitask heads.

A result below the linear baseline suggests improving the representation or
training objective before making the encoder larger. A repeatable improvement
above the linear baseline supports adding `subsystem`, `function_family` and
confidence heads to the same embedding.
