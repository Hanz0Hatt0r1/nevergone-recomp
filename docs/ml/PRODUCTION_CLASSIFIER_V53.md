# Production Classifier v5.3: nested game verifier experiment

v5.2 established that normalized receiver ratios did not materially improve the main
classifier beyond v5.1:

```text
v4   balanced=0.8160 macro_f1=0.7370 accept_precision=0.9661 accept_coverage=0.6023 game_precision=0.3164
v5   balanced=0.8281 macro_f1=0.7462 accept_precision=0.9664 accept_coverage=0.5741 game_precision=0.3756
v5.1 balanced=0.8334 macro_f1=0.7544 accept_precision=0.9669 accept_coverage=0.6302 game_precision=0.3824
v5.2 balanced=0.8333 macro_f1=0.7544 accept_precision=0.9671 accept_coverage=0.6328 game_precision=0.3824
```

The v5.2 boundary audit found:

```text
game -> engine: 133
engine -> game: 595
```

The dominant remaining error is therefore not insufficient generic feature capacity;
it is false `game` promotion from `engine` functions.

## Experiment

v5.3 adds a secondary verifier only for the `game`/`engine` boundary. It does not
replace the four-class classifier and it does not currently alter production routing.

The verifier is trained only on true `game` and `engine` examples using the same
portable P-code + v5.2 objective evidence representation. It is used in one direction:

```text
base prediction != game
    -> unchanged

base prediction == game
    -> verifier P(game) >= threshold -> keep game
    -> verifier P(game) <  threshold -> demote to engine
```

This asymmetry is intentional because `engine -> game` errors outnumber
`game -> engine` errors by more than four to one in the current OOF audit.

## Leakage control

The evaluator is nested and family-grouped.

For each outer family-held-out fold:

1. only outer-train `game`/`engine` rows are used to fit the verifier;
2. the verifier threshold is selected from inner family-grouped OOF predictions;
3. the outer test families are not used for verifier fitting or threshold selection;
4. the selected threshold is then applied only to base OOF `game` predictions in the
   outer test fold.

The experiment is post-hoc. Its output does not feed labels or error identities back
into v5.2 training.

The verifier representation keeps the existing leakage policy: no raw function names,
source addresses, receiver class names or vtable identities are introduced.

## Run

The experiment reuses the already-created v5.2 evidence and OOF predictions. No new
Ghidra run is required.

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
source ~/.venvs/nevergone-ml/bin/activate

bash tools/ml/run_nevergone_game_verifier_v53.sh \
  ~/.local/share/nevergone-ml-v2 \
  | tee ~/.local/share/nevergone-ml-v2/logs/v53-full-run.txt
```

Inputs:

```text
~/.local/share/nevergone-ml-v2/dataset-4class/
~/.local/share/nevergone-ml-v2/families.jsonl
~/.local/share/nevergone-ml-v2/evidence-v5.2/evidence.jsonl
~/.local/share/nevergone-ml-v2/models/production-code-domain-v5.2/oof_predictions.jsonl
```

Outputs:

```text
~/.local/share/nevergone-ml-v2/evidence-v5.3-game-verifier/summary.json
~/.local/share/nevergone-ml-v2/evidence-v5.3-game-verifier/oof_verifier_predictions.jsonl
```

## Promotion rule

This experiment should only be promoted into a serialized production classifier if it
shows a meaningful family-held-out improvement. The preferred outcome is:

```text
engine -> game errors  materially lower than 595
game precision          > 0.40
balanced accuracy       >= v5.2 0.8333
macro F1                >= v5.2 0.7544
helpful flips           clearly greater than harmful flips
```

A verifier that improves game precision only by collapsing game recall is rejected.
If the candidate passes, the next step is to integrate the verifier into a production
artifact and recalibrate confidence/OOD routing after the final prediction rather than
using the base v5.2 routing unchanged.
