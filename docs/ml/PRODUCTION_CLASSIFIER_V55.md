# Production Classifier v5.5: Pareto game verifier

v5.5 is an evaluation-only continuation of the v5.4 selective `game <-> engine`
verifier. It does not change the serialized production classifier or routing policy.

Reference local v5.4 result:

```text
base v5.2:
  balanced=0.8333 macro_f1=0.7544
  game_precision=0.3824 game_recall=0.7300 game_f1=0.5019

v5.4 selective:
  balanced=0.8336 macro_f1=0.7650
  game_precision=0.4244 game_recall=0.7058 game_f1=0.5301
  engine->game: 595 -> 478
  game->engine: 133 -> 146
  flips=132 helpful=117 harmful=13
  v5.2 OOF reproduction rate=1.0
```

The v5.4 verifier is accurate when it intervenes, but conservative. v5.5 expands
policy search without changing the base four-class model or receiver evidence.

## Nested evaluation

For every outer `StratifiedGroupKFold` split v5.5:

1. rebuilds the four-class v5.2-style classifier from outer-train only;
2. rebuilds the binary `game`/`engine` verifier from outer-train only;
3. creates inner family-grouped OOF base and verifier probabilities;
4. evaluates candidate policies only on those inner OOF rows;
5. applies the selected policy to the untouched outer-test families.

The checked-in v5.2 OOF file is used only to audit reproduction. It is never an
input to policy selection.

## Policy families

`single_margin` is the v5.4-style rule:

```text
base_prediction == game
AND base P(game)-P(engine) <= margin_limit
AND verifier P(game) < threshold
=> engine
```

`confidence_ceiling` protects strong base `game` predictions using the absolute
base game probability rather than only the pairwise margin.

`two_stage_margin` uses two verifier thresholds. Very weak base `game` predictions
can be demoted with a stricter verifier requirement, while medium-strength base
predictions use a milder threshold. High-margin `game` predictions are untouched.

A `noop` policy is always present as a safe fallback.

## Constraints and Pareto selection

Default hard inner-OOF constraints are:

```text
balanced_accuracy >= inner_base - 0.003
game_recall       >= inner_base - 0.04
```

The default target game precision is `0.45`. Eligible candidates are reduced to a
Pareto frontier over balanced accuracy, macro-F1, game precision/recall,
helpful-minus-harmful flips, harmful flips and remaining `engine->game` errors.

If any Pareto candidate reaches the target precision, selection is restricted to
those candidates. The final ranking prioritizes macro-F1, then game precision,
balanced accuracy, game F1, net helpful flips, fewer `engine->game` errors and
fewer harmful flips.

All thresholds are selected on inner OOF only. Outer labels never select a policy.

## Run

No Ghidra pass is required. v5.5 reuses the persistent v5.2 evidence and dataset:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
source ~/.venvs/nevergone-ml/bin/activate

bash tools/ml/run_nevergone_pareto_game_verifier_v55.sh \
  ~/.local/share/nevergone-ml-v2 \
  | tee ~/.local/share/nevergone-ml-v2/logs/v55-full-run.txt
```

Outputs:

```text
~/.local/share/nevergone-ml-v2/evidence-v5.5-pareto-game-verifier/summary.json
~/.local/share/nevergone-ml-v2/evidence-v5.5-pareto-game-verifier/oof_pareto_verifier_predictions.jsonl
```

Useful promotion criteria are approximately:

```text
balanced_accuracy >= 0.830
macro_f1          >= 0.770
game_precision    >= 0.45, ideally 0.48-0.50
game_recall       >= 0.68
engine->game      materially below v5.4's 478
harmful flips     remains well below helpful flips
v5.2 reproduction rate == 1.0
```

Even if v5.5 succeeds, production inference should only be changed in a separate
packaging step followed by fresh calibration of accept/review/OOD routing and a
frozen unseen/OOD validation run.
