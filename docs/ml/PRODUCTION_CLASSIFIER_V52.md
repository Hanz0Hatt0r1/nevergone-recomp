# Production Classifier v5.2: normalized receiver ratios

`ProductionClassifier/5.2` keeps the `NEVERGONE.ProductionClassifier/5` serialized
model contract and the v5.1 mass receiver export. The experiment changes only the
receiver evidence representation and adds a post-hoc `game <-> engine` boundary audit.

Reference run before this change:

```text
v4   balanced=0.8160 macro_f1=0.7370 accept_precision=0.9661 accept_coverage=0.6023
v5   balanced=0.8281 macro_f1=0.7462 accept_precision=0.9664 accept_coverage=0.5741
v5.1 balanced=0.8334 macro_f1=0.7544 accept_precision=0.9669 accept_coverage=0.6302
```

The v5.1 run matched 3,499 receiver type rows and 3,499 receiver inference rows to
the current 9,118-function dataset. That proved receiver/type evidence was useful,
but the evidence was represented mainly as absolute per-function counts. Absolute
counts can partly encode function size, so v5.2 adds normalized ratios without
removing the existing counts.

## Added receiver features

Ratios use the receiver CALLIND count or inference row count as their denominator.
Examples:

```text
receiver.present_ratio
receiver.direct_single_pointer_ratio
receiver.flow_unique_ratio
receiver.flow_ambiguous_ratio
receiver.pointer_depth_mean
receiver.pointer_depth_1_ratio
receiver.storage_unique_ratio
receiver.storage_register_ratio
receiver.inference_to_call_ratio
receiver.resolved_ratio
receiver.runtime_match_ratio
receiver.non_virtual_ratio
receiver.status_*_ratio
receiver.evidence_*_ratio
receiver.slot_*_ratio
receiver.candidate_class_mean
receiver.flow_candidate_mean
```

Division-by-zero rows simply do not receive a ratio feature. Raw class names,
function names, source addresses, exact vtable identities and labels are not emitted
as model features.

The count evidence is first built exactly as in v5.1, then
`augment_nevergone_v52_receiver_ratios.py` creates a second evidence file. This keeps
v5.1 reproducible and makes v5.1 -> v5.2 a controlled representation ablation.

## Game/engine boundary audit

`audit_nevergone_game_engine_boundary.py` consumes family-held-out OOF predictions
after training. It records only mistakes where both truth and prediction are one of
`game` or `engine`, plus sanitized receiver evidence summaries.

The audit reports:

- `game -> engine` and `engine -> game` error counts;
- confidence, probabilities and routing actions;
- top family IDs involved in each transition;
- mean receiver features for each transition;
- largest receiver-feature deltas between errors and correctly classified rows with
  the same truth class.

This is explicitly post-hoc analysis. Truth and prediction labels never feed back
into the training representation.

## Run

v5.2 reuses the persistent v5.1 receiver artifacts. A second Ghidra pass is not
required:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
source ~/.venvs/nevergone-ml/bin/activate

bash tools/ml/run_nevergone_production_classifier_v52.sh \
  ~/.local/share/nevergone-ml-v2 \
  | tee ~/.local/share/nevergone-ml-v2/logs/v52-full-run.txt
```

Required receiver inputs are:

```text
~/.local/share/nevergone-ml-v2/receiver-v5.1/indirect-receiver-types.tsv
~/.local/share/nevergone-ml-v2/receiver-v5.1/static-receiver-vtable-inference-mass.jsonl
```

Outputs:

```text
~/.local/share/nevergone-ml-v2/evidence-v5.2/evidence-base.jsonl
~/.local/share/nevergone-ml-v2/evidence-v5.2/evidence.jsonl
~/.local/share/nevergone-ml-v2/evidence-v5.2/game-engine-boundary-errors.jsonl
~/.local/share/nevergone-ml-v2/evidence-v5.2/game-engine-boundary-summary.json
~/.local/share/nevergone-ml-v2/models/production-code-domain-v5.2/
```

The runner prints v4/v5/v5.1/v5.2 balanced accuracy, macro-F1, accept precision,
accept coverage and game precision from the same family-grouped CV policy.

## Optional frozen unseen/OOD hooks

The runner can execute inference on already prepared frozen corpora after training.
For a fair v5.2 comparison, provide evidence built with the same feature categories:

```bash
V52_UNSEEN_CORPUS=/path/to/unseen.jsonl \
V52_UNSEEN_EVIDENCE=/path/to/unseen-evidence-v52.jsonl \
bash tools/ml/run_nevergone_production_classifier_v52.sh \
  ~/.local/share/nevergone-ml-v2
```

Equivalent variables are available for external OOD data:

```text
V52_OOD_CORPUS
V52_OOD_EVIDENCE
```

These hooks do not fabricate missing evidence. If only a corpus is supplied,
inference runs with portable P-code plus derived structural evidence and must be
interpreted as a partial-evidence test.

## Promotion criteria

v5.2 should beat or match v5.1 on the same frozen folds while preserving production
routing quality. Current targets are:

```text
balanced accuracy   > 0.835
macro F1            > 0.755
accept precision    >= 0.97 preferred, no regression below the v5.1 band
accept coverage     >= 0.63
game precision      > 0.40 target
```

If ratios do not improve held-out metrics, keep the v5.1 count representation and
use the boundary audit to decide which new independent game families or deterministic
evidence should be collected next.
