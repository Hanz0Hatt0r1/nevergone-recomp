# Frozen unseen-500 validation for v10

`tools/ml/run_nevergone_frozen_unseen500_v10.sh` creates a persistent,
deterministic validation corpus that is identity-disjoint from the current
`dataset-4class` splits, builds the same evidence families used by v5.2/v10, and
runs the packaged accept-error router.

This replaces the old temporary `/tmp` unseen export, whose exact bytes were not
preserved. The new corpus is frozen under the persistent ML workspace and guarded
by SHA-256 hashes. Re-running the script reuses the frozen bytes unless
`V10_REFRESH_FROZEN=1` is explicitly set.

## What this validates

The 500 selected functions are removed if their stable source identity occurs in
any current training/validation/test split. Selection then uses the existing
stable hash ranking, rather than a contiguous address window.

The runner builds:

- canonical P-code corpus and trace;
- full-index callgraph metrics;
- bounded mass receiver/type export with `-noanalysis`;
- static receiver/vtable inference;
- v5.2 receiver-ratio evidence;
- v10 routed classifications;
- an immutable corpus manifest and a validation summary;
- a distribution-shift audit against the v5.2/v10 OOF reference.

The holdout has no independent semantic labels. Therefore it validates identity
separation, feature/evidence availability, routing stability and OOD/deployment
behavior. It must **not** be reported as an unbiased semantic precision estimate.
The v9 cross-fitted OOF result remains the labeled routing estimate.

## Run

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
source ~/.venvs/nevergone-ml/bin/activate

bash tools/ml/run_nevergone_frozen_unseen500_v10.sh \
  ~/.local/share/nevergone-ml-v2 \
  | tee ~/.local/share/nevergone-ml-v2/logs/v10-frozen-unseen500.txt
```

Default local Ghidra settings match the existing Nevergone ML runners:

```text
GHIDRA_HOME=/opt/ghidra
GHIDRA_PROJECT_DIR=/home/pes/ghidra_projects/nevergone
GHIDRA_PROJECT_NAME=nevergone
GHIDRA_PROGRAM=libcocos2dcpp.so
```

They can be overridden through environment variables.

The first export is bounded to 2000 functions. If this does not contain 500
eligible unseen internal functions the runner retries with 4000. The selected
corpus is then frozen; later runs do not reselect it.

Output root:

```text
~/.local/share/nevergone-ml-v2/frozen-validation/unseen500-v10/
```

Important files:

```text
manifest.json
corpus.jsonl
trace.jsonl
evidence.jsonl
indirect-receiver-types.tsv
static-receiver-vtable-inference.jsonl
v10.classified.jsonl
validation-summary.json
drift-summary.json
```

`validation-summary.json` records corpus SHA-256, prediction/action counts,
router change rate and callgraph/receiver evidence coverage.

## Distribution-shift audit

`tools/ml/audit_nevergone_frozen_drift_v10.py` compares the frozen set against
the labeled OOF reference without using frozen labels. It reports:

- Jensen-Shannon divergence for predicted-class, base-action and routed-action
  distributions;
- confidence, raw-confidence, family-similarity, competing-similarity,
  contrast-margin, max-probability, top-2-gap and entropy shifts;
- the router downgrade rate among legacy `accept` rows;
- callgraph/receiver evidence coverage changes;
- the largest per-feature evidence-presence shifts.

The audit emits a `low`, `moderate` or `high` drift indicator using documented
heuristic thresholds. That indicator is diagnostic only: it is not a semantic
accuracy estimate and it must not be used as a replacement for labeled
validation.

The production reference intervention rate is derived from the v10 cross-fitted
calibration (`legacy_accept_examples - kept_examples`). Frozen rows are compared
against that reference, so a higher downgrade rate can indicate appropriate
caution on new identities rather than model failure.

## Refresh policy

Do not refresh the corpus after inspecting model behavior. A refresh changes the
holdout population and invalidates longitudinal comparisons. Only create a new
frozen generation deliberately:

```bash
V10_REFRESH_FROZEN=1 \
  bash tools/ml/run_nevergone_frozen_unseen500_v10.sh \
  ~/.local/share/nevergone-ml-v2
```

When the model is later generalized to another game/architecture, prefer a
separate cross-project frozen corpus rather than repeatedly resampling this one.
