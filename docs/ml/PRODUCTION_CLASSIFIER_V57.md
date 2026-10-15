# Production Classifier v5.7: constrained GAME meta calibration

v5.7 is an evaluation-only continuation of the v5.6 structured GAME trust meta-classifier.
It does not change production inference or routing.

## Motivation

Real v5.6 family-held-out results showed that the meta-classifier has strong signal:

- base v5.2: balanced `0.8333`, macro-F1 `0.7544`, GAME precision `0.3824`, GAME recall `0.7300`;
- v5.6 fixed threshold 0.50: balanced `0.8066`, macro-F1 `0.7902`, GAME precision `0.7003`, GAME recall `0.5177`;
- `engine->game` errors fell from `595` to `109`.

The model therefore finds false GAME predictions well, but threshold 0.50 is too aggressive.
v5.7 keeps the model and calibrates only how often it is allowed to demote GAME to ENGINE.

## Leakage-safe nested calibration

For every outer family-held-out fold:

1. Rebuild the v5.2 base classifier on inner family-grouped folds.
2. Produce inner-OOF base probabilities for the entire outer-train set.
3. Build meta candidates only where the inner-OOF base prediction is `game`.
4. Run another family-grouped CV over those candidates to produce **meta-OOF keep probabilities**.
5. Select the GAME keep threshold using only those meta-OOF predictions and outer-train labels.
6. Enforce hard constraints relative to the inner base model:
   - balanced accuracy loss <= `0.003`;
   - GAME recall loss <= `0.03`.
7. Prefer thresholds reaching GAME precision `0.45`, then rank by macro-F1, GAME F1, precision and net helpful flips.
8. Fit the final meta-model on all outer-train OOF candidates only after the threshold is frozen.
9. Evaluate once on untouched outer-test families.

The outer test fold never participates in base fitting, meta fitting or threshold selection.

## Features

v5.7 reuses v5.6 features:

- four base class probabilities;
- GAME-ENGINE margin;
- top-2 probability gap;
- entropy;
- base confidence;
- `1 - P(GAME)`;
- sanitized numeric production evidence from v5.2.

Evidence keys containing raw identity-like fields are excluded defensively, including labels, raw class names, vtable identities, addresses, source entries, function names and symbol names.

## Run

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
source ~/.venvs/nevergone-ml/bin/activate

bash tools/ml/run_nevergone_constrained_game_meta_v57.sh \
  ~/.local/share/nevergone-ml-v2 \
  | tee ~/.local/share/nevergone-ml-v2/logs/v57-full-run.txt
```

No Ghidra rerun is required. Existing v5.2 evidence and OOF predictions are reused.

Outputs:

```text
~/.local/share/nevergone-ml-v2/evidence-v5.7-constrained-game-meta/summary.json
~/.local/share/nevergone-ml-v2/evidence-v5.7-constrained-game-meta/oof_constrained_meta_predictions.jsonl
```

## Promotion gate

v5.4 remains the frozen champion until v5.7 beats it on the same family-held-out protocol.
Reference v5.4 metrics:

```text
balanced       0.8336
macro-F1       0.7650
GAME precision 0.4244
GAME recall    0.7058
engine->game   478
game->engine   146
```

Useful v5.7 target region:

```text
balanced       >= 0.832
macro-F1       > 0.765
GAME precision >= 0.45
GAME recall    >= 0.70
engine->game   < 478
```

If v5.7 succeeds, the next step is production packaging and recalibration of accept/review/OOD after the meta decision, not another policy-search experiment.
