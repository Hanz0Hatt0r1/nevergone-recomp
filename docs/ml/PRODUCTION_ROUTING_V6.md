# Production routing v6: GAME meta gate

v5.4 remains the best hard-label correction experiment. v5.6 showed a much stronger
signal for false `game` predictions, but using that signal to relabel functions was
too aggressive. v5.7 constrained calibration preserved recall by almost disabling
the meta verifier.

v6 therefore changes the objective: **do not relabel anything**. Keep the v5.2
class prediction fixed and use the v5.6 GAME trust probability only as a routing
signal for `accept/review/escalate`.

## Motivation

Reference real results:

```text
v5.2 base
balanced       0.8333
macro-F1       0.7544
accept prec.   about 0.967
accept cov.    about 0.633

v5.6 hard relabel
balanced       0.8066
macro-F1       0.7902
GAME precision 0.7003
GAME recall    0.5177
engine->game   109

v5.7 constrained hard relabel
balanced       0.8329
macro-F1       0.7563
GAME precision 0.3899
GAME recall    0.7225
```

The v5.6 score is highly informative, but the correct production use may be to
reduce false automatic acceptance rather than to overwrite the semantic class.

## Leakage-safe routing calibration

The evaluator joins:

- v5.2 `oof_predictions.jsonl` for the frozen class prediction and existing
  `accept/review/escalate` action;
- v5.6 `oof_meta_verifier_predictions.jsonl` for outer-held-out
  `meta_keep_game_probability` and fold IDs.

For every target fold, GAME routing thresholds are selected using only the other
outer folds. The target fold labels do not participate in threshold selection.
No class prediction can change and routing can only become more conservative:

```text
base GAME + accept
  high trust   -> accept
  medium trust -> review
  low trust    -> escalate

base GAME + review
  enough trust -> review
  low trust    -> escalate

non-GAME rows and existing escalations are unchanged
```

The default target is:

```text
accept precision >= 0.97
review precision >= 0.80
accept coverage loss <= 0.04
```

If no threshold pair is feasible on a calibration split, that fold falls back to
no-op routing.

## Run

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
source ~/.venvs/nevergone-ml/bin/activate

bash tools/ml/run_nevergone_game_meta_routing_v6.sh \
  ~/.local/share/nevergone-ml-v2 \
  | tee ~/.local/share/nevergone-ml-v2/logs/v6-full-run.txt
```

No Ghidra rerun is required.

Outputs:

```text
~/.local/share/nevergone-ml-v2/evidence-v6-game-meta-routing/summary.json
~/.local/share/nevergone-ml-v2/evidence-v6-game-meta-routing/oof_meta_routing_predictions.jsonl
```

## Promotion gate

v6 is useful if it reaches the production precision target while retaining most
of the existing automatic coverage:

```text
accept precision >= 0.970
accept coverage  >= 0.600
class metrics     unchanged exactly
```

A particularly strong result would be accept precision above 0.975 with coverage
at or above 0.61. If this succeeds, the next step is to package the GAME meta model
and the learned routing gate into a production artifact and validate it on frozen
unseen/OOD functions.
