# Production routing v7: all-class correctness meta router

v7 changes the production problem rather than tuning another `GAME -> ENGINE` relabeling threshold.

The semantic prediction remains frozen. A second-stage model estimates the probability that the current base prediction is correct and uses that probability only for `accept / review / escalate` routing.

## Why v6 could not reach 0.97 accept precision

The real v6 run had:

```text
accept examples   5770
accept precision  0.9671
GAME accepts      94
GAME precision    0.9362
```

That is roughly 190 total accepted errors, but only 6 errors among accepted `GAME` predictions. Even a perfect GAME-only filter cannot remove enough accepted errors to lift global precision from `0.9671` to `0.9700`.

Therefore v7 must reason about accepted predictions from all four classes.

## Protocol

For each outer family-held-out fold:

1. Build inner family-grouped OOF predictions from the v5.2 representation (`portable P-code + evidence-v5.2`).
2. Learn the legacy v5 routing policy from those inner OOF predictions for a same-fold baseline.
3. Build structured correctness features from the inner OOF base output:
   - all four class probabilities;
   - top-2 probability gap;
   - entropy and max probability;
   - predicted-family similarity;
   - competing-family similarity;
   - contrastive margin;
   - predicted-class one-hot;
   - sanitized numeric evidence-v5.2 features.
4. The meta target is `1` when the inner-OOF base prediction is correct and `0` otherwise.
5. Produce meta-level OOF correctness probabilities with another family-grouped split inside the outer training set.
6. Select `accept` and `review` correctness thresholds only from those meta-OOF rows.
7. Fit the final correctness meta model on all outer-train inner-OOF rows after thresholds are frozen.
8. Evaluate once on untouched outer-test families.

The outer test fold never participates in base fitting, meta fitting, or threshold selection.

## Meta model

v7 uses balanced logistic regression with standardized structured features. It fits:

- one global correctness model;
- optional per-predicted-class correctness models when enough examples exist.

Per-class models allow `game`, `engine`, `runtime_library`, and `media_library` to have different correctness boundaries while retaining the global model as fallback.

No raw function names, source addresses, receiver class names, or vtable identities are included.

## Routing target

Default v7 meta-routing goals:

```text
accept precision >= 0.970
review precision >= 0.800
minimum accept examples per calibration fold = 50
```

Among feasible threshold pairs, v7 maximizes accept coverage first, then accept precision and review coverage.

If a fold cannot prove a feasible meta-routing policy from its outer-train meta-OOF rows, that fold falls back to the nested legacy routing policy instead of manufacturing an empty high-precision accept bucket.

## Diagnostics

v7 reports accept attribution by predicted class for both legacy and meta routing:

```text
game:            examples / correct / errors / precision
engine:          examples / correct / errors / precision
runtime_library: examples / correct / errors / precision
media_library:   examples / correct / errors / precision
```

This identifies which prediction classes dominate residual accepted errors.

## Run

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
source ~/.venvs/nevergone-ml/bin/activate

bash tools/ml/run_nevergone_correctness_meta_routing_v7.sh \
  ~/.local/share/nevergone-ml-v2 \
  | tee ~/.local/share/nevergone-ml-v2/logs/v7-full-run.txt
```

No Ghidra rerun is required. The existing dataset, families, evidence-v5.2 and v5.2 OOF file are reused.

Outputs:

```text
~/.local/share/nevergone-ml-v2/evidence-v7-correctness-meta-routing/summary.json
~/.local/share/nevergone-ml-v2/evidence-v7-correctness-meta-routing/oof_correctness_meta_routing.jsonl
```

## Promotion gate

The main production objective is selective routing, not hard-label accuracy:

```text
accept precision >= 0.970
accept coverage  >= 0.600
```

Class predictions are frozen, so balanced accuracy and macro-F1 should match v5.2 exactly. `v5.2 OOF reproduction rate` should be `1.0`.

If v7 reaches the routing gate under this fully nested protocol, the next step is to package the correctness meta model and its thresholds into the production artifact and then run the frozen unseen/OOD evaluation.
