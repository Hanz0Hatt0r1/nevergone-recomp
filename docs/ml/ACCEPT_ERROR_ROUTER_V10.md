# Accept-error router v10

v10 packages the v9 accept-only meta-router for production inference. It does **not**
replace the semantic ProductionClassifier/5 artifact and it does not claim a new
held-out metric. The unbiased reference remains the v9 cross-fitted result.

## Why package v9

The real v9 run kept 5204 accepts at 0.9698309 precision and 0.5707 coverage, versus
5388 at 0.9599 for the nested legacy route. That misses the nominal 0.9700 target by
one accepted error while preserving useful coverage. Further OOF threshold tuning
would risk optimizing directly against the evaluation set.

v10 therefore freezes the architecture and creates one deployable router:

1. Rebuild the existing v5.2/v8 OOF accept population.
2. Produce cross-fitted accept-correctness scores: each outer fold is scored by a
   meta-model trained on other folds only.
3. Select one global production threshold from those cross-fitted scores.
4. Fit final global/per-predicted-class logistic correctness models on all legacy
   ACCEPT OOF rows.
5. Serialize `NEVERGONE.AcceptErrorRouter/1`.

The router is monotone. It can only preserve ACCEPT or downgrade it to ESCALATE.
Legacy REVIEW and ESCALATE are preserved exactly. Promotions are impossible.

## Build

```bash
bash tools/ml/run_nevergone_accept_error_router_v10.sh \
  ~/.local/share/nevergone-ml-v2
```

Outputs:

```text
~/.local/share/nevergone-ml-v2/models/accept-error-router-v10/
  accept_error_router.joblib
  manifest.json
```

## Routed inference

```bash
python3 tools/ml/classify_nevergone_pcode_with_accept_router.py \
  ~/.local/share/nevergone-ml-v2/models/production-code-domain-v5.2 \
  ~/.local/share/nevergone-ml-v2/models/accept-error-router-v10 \
  /path/to/corpus.jsonl \
  --evidence /path/to/evidence.jsonl \
  --output /tmp/classified-v10.jsonl
```

Each result keeps the base classifier fields and adds:

- `base_action`
- `accept_router_format`
- `accept_router_threshold`
- `accept_correctness_probability`
- `accept_router_changed`

## Frozen unseen/OOD

The runner accepts optional frozen corpora without hard-coding local paths:

```bash
V10_UNSEEN_CORPUS=/path/to/unseen.jsonl \
V10_UNSEEN_EVIDENCE=/path/to/unseen-evidence.jsonl \
V10_OOD_CORPUS=/path/to/ood.jsonl \
V10_OOD_EVIDENCE=/path/to/ood-evidence.jsonl \
bash tools/ml/run_nevergone_accept_error_router_v10.sh \
  ~/.local/share/nevergone-ml-v2
```

Frozen sets must use evidence categories compatible with the v5.2 evidence schema.
Do not tune the packaged threshold on frozen unseen/OOD results.
