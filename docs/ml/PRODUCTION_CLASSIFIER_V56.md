# Production Classifier v5.6: structured GAME meta verifier

v5.6 is an evaluation-only continuation after v5.4/v5.5. It does not modify the serialized production classifier or production routing.

## Why v5.6 exists

The v5.5 Pareto policy search did not beat the v5.4 champion. Reference results from the real 9,118-function run:

- v5.4: balanced accuracy 0.8336, macro-F1 0.7650, GAME precision 0.4244, GAME recall 0.7058, engine->game 478, game->engine 146.
- v5.5: balanced accuracy 0.8304, macro-F1 0.7625, GAME precision 0.4177, GAME recall 0.6946, engine->game 486, game->engine 152.

This indicates that further hand-written threshold search is not the best next step. v5.6 instead learns a small second-stage classifier that answers one narrower question: when the four-class base model predicts `game`, should that prediction be trusted?

## Leakage policy

The evaluator uses outer family-grouped cross-validation. Inside each outer-train split, it reconstructs the v5.2 base model using inner family-grouped folds and generates inner-OOF base probabilities. Only rows whose inner-OOF base prediction is `game` become meta-training examples. The target is `1` when the true class is `game` and `0` otherwise.

Therefore, the base probability features for every meta-training row were produced by a base model that did not train on that row. The outer test fold is never used to fit either the base model or the meta model.

The meta model receives:

- four aligned base probabilities;
- GAME minus engine margin;
- top-two probability gap;
- prediction entropy;
- maximum base probability;
- one minus GAME probability;
- sanitized numeric production evidence from `evidence-v5.2/evidence.jsonl`.

Evidence keys containing label, raw class-name, raw vtable identity, address, source-entry, function-name or symbol-name markers are excluded defensively. The v5.2 evidence builder already avoids those identities; this is an additional guard.

The v5.6 classifier is a standardized, class-balanced logistic regression. It is intentionally small and interpretable. The keep/demote threshold defaults to the fixed value `0.50`; it is not tuned on outer labels.

## Run

```bash
source ~/.venvs/nevergone-ml/bin/activate
bash tools/ml/run_nevergone_game_meta_verifier_v56.sh \
  ~/.local/share/nevergone-ml-v2
```

No Ghidra rerun is required. It reuses:

- `dataset-4class/`
- `families.jsonl`
- `evidence-v5.2/evidence.jsonl`
- `models/production-code-domain-v5.2/oof_predictions.jsonl`
- optional v5.4 summary for reporting only.

Outputs:

```text
~/.local/share/nevergone-ml-v2/evidence-v5.6-game-meta-verifier/summary.json
~/.local/share/nevergone-ml-v2/evidence-v5.6-game-meta-verifier/oof_meta_verifier_predictions.jsonl
~/.local/share/nevergone-ml-v2/logs/v56-game-meta-verifier.log
```

## Promotion gate

v5.4 remains the champion until v5.6 beats it on family-held-out OOF. The preferred gate is:

- balanced accuracy >= 0.8320;
- macro-F1 > 0.7650;
- GAME precision > 0.4244;
- GAME recall >= 0.7000;
- engine->game < 478;
- game->engine <= 150;
- v5.2 OOF reproduction rate = 1.0.

A stronger target is macro-F1 >= 0.7700 and GAME precision >= 0.45 while preserving GAME recall >= 0.70.
