# Production Classifier v5.4: constrained selective game verifier

v5.3 proved that a dedicated `game` / `engine` verifier can remove a large share of
false `game` predictions, but the unconstrained demotion policy was too aggressive:

```text
base v5.2: balanced=0.8333 macro_f1=0.7544 game_precision=0.3824
v5.3:      balanced=0.8128 macro_f1=0.7875 game_precision=0.6237

base boundary errors:     engine->game=595 game->engine=133
v5.3 boundary errors:     engine->game=140 game->engine=230
v5.3 flips:               552 helpful=455 harmful=97
```

The verifier itself is useful: 82.4% of its interventions fixed an error. The failure
mode was policy selection. v5.4 therefore keeps the binary verifier but applies it
only to weak base `game` decisions.

## Leakage-resistant nested evaluation

The evaluator reconstructs the v5.2 base classifier inside the same outer
`StratifiedGroupKFold(seed=42)` scheme used by the production trainer. For every
outer fold it then creates a second inner family-grouped CV over the outer training
set.

Inner folds produce two held-out signals:

- four-class v5.2 base probabilities;
- binary `game` / `engine` verifier probabilities.

Policy selection sees only those inner OOF predictions. The outer test families are
not available to either model or to policy selection.

The precomputed v5.2 `oof_predictions.jsonl` is not used for policy fitting. It is
loaded only after nested evaluation so the evaluator can report the reproduction
rate of its independently reconstructed v5.2 base predictions.

## Selective policy

A base prediction is changed only when all conditions hold:

```text
base prediction == game
P(game) - P(engine) <= base margin limit
verifier P(game) < verifier threshold
```

The inner search grid is intentionally small and fixed:

```text
verifier threshold: 0.40 .. 0.80
base game-engine margin limit: 0.05, 0.10, 0.15, 0.20, 0.30, 0.50, 1.00
```

A no-op policy is always included as a safe fallback.

Candidates are eligible only when the inner held-out results satisfy:

```text
balanced_accuracy >= base_balanced_accuracy - 0.003
game_recall       >= base_game_recall       - 0.03
```

The limits can be overridden for experiments, but the outer fold is never consulted
when choosing them. Among eligible candidates the selector prioritizes game
precision, then macro-F1, balanced accuracy, net helpful flips, fewer harmful flips,
and fewer total interventions.

## Run

No Ghidra rerun is required. v5.4 reuses the current v5.2 evidence:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
source ~/.venvs/nevergone-ml/bin/activate

bash tools/ml/run_nevergone_selective_game_verifier_v54.sh \
  ~/.local/share/nevergone-ml-v2 \
  | tee ~/.local/share/nevergone-ml-v2/logs/v54-full-run.txt
```

Optional constraint overrides:

```bash
V54_MAX_BALANCED_LOSS=0.005 \
V54_MAX_GAME_RECALL_LOSS=0.04 \
bash tools/ml/run_nevergone_selective_game_verifier_v54.sh \
  ~/.local/share/nevergone-ml-v2
```

Outputs:

```text
~/.local/share/nevergone-ml-v2/evidence-v5.4-selective-game-verifier/summary.json
~/.local/share/nevergone-ml-v2/evidence-v5.4-selective-game-verifier/oof_selective_verifier_predictions.jsonl
~/.local/share/nevergone-ml-v2/logs/v54-selective-game-verifier.log
```

## Promotion rule

v5.4 remains evaluation-only. Do not change serialized production routing until the
nested run shows a useful precision gain without materially reducing balanced
accuracy or game recall. A desirable result is approximately:

```text
balanced accuracy >= 0.830
macro-F1         > 0.77
game precision   > 0.50
engine->game     < 300
harmful flips    < 40
```
