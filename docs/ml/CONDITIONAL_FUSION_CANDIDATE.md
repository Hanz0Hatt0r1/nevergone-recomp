# Frozen conditional-fusion candidate

The readiness gate answers only whether a predeclared conditional-fusion policy
surface is worth testing independently. It must not be wired directly into
production because the readiness decision itself was made from nested
family-held-out development results.

This stage freezes an exact candidate **before** any independent validation data
is inspected.

## Inputs

The builder joins five artifacts:

1. `NEVERGONE.ConditionalFusionReadiness/1` with
   `ready_for_independent_validation=true`;
2. a `NEVERGONE.ProductionClassifier/4` manifest;
3. that classifier's `oof_predictions.jsonl`;
4. a `NEVERGONE.SimilarityTransferOOF/1` manifest;
5. the matching similarity-transfer OOF predictions.

The classifier and transfer evidence must describe the exact same development
corpus. The builder therefore requires identical dataset fingerprints, function
counts, family counts, row identities, family IDs and truth labels. Any mismatch
fails closed.

## Transfer OOF evidence

`export_nevergone_similarity_transfer_oof.py` produces one family-held-out
nearest-family retrieval result per labelled development function. It uses the
same portable P-code text surface as the existing nested similarity-transfer
experiments and records:

- predicted label;
- correctness;
- nearest-family similarity;
- top-vs-second similarity gap;
- nearest family ID.

The export manifest records the same `_sha256_rows` dataset fingerprint used by
ProductionClassifier/4, so evidence from different dataset versions cannot be
silently joined.

## Freezing the candidate

`build_nevergone_conditional_fusion_candidate.py` first learns the base transfer
policy from the full development OOF rows. Only transfer classes already marked
trusted by the readiness artifact can participate.

It then considers exactly the readiness-approved `label/action` keys. For each
key it selects the highest-coverage similarity/gap pair reaching the configured
precision target on full-development OOF evidence. The default grid and safety
target remain the existing experimental values:

```text
transfer precision target:    0.95
conditional precision target: 0.95
minimum evidence per policy:  10
```

A readiness-approved key that cannot still satisfy the freeze-time development
OOF gate causes the build to fail. It is not silently dropped or weakened.

The resulting artifact is:

`NEVERGONE.ConditionalFusionCandidate/1`

and contains exact:

- trusted transfer classes;
- readiness-approved policy keys;
- transfer similarity/gap thresholds;
- conditional similarity/gap thresholds;
- development OOF support and precision;
- hashes of readiness, production manifest and transfer OOF manifest;
- the development dataset fingerprint.

It explicitly records:

```text
validation_state = pending_independent_validation
production_routing_changed = false
```

## One-command packaging

After the readiness runner passes and a matching ProductionClassifier/4 artifact
exists:

```bash
cd ~/nevergone-recomp
source /tmp/nevergone-ml-venv/bin/activate

bash tools/ml/run_nevergone_conditional_fusion_candidate.sh \
  /tmp/nevergone-ml-v2/conditional-fusion-readiness/conditional-fusion-readiness.json \
  /tmp/nevergone-ml-v2/production-code-domain-v4 \
  /tmp/nevergone-ml-v2/dataset-4class \
  /tmp/nevergone-ml-v2/families.jsonl \
  /tmp/nevergone-ml-v2/conditional-fusion-candidate
```

Outputs:

```text
conditional-fusion-candidate.json
transfer-oof/manifest.json
transfer-oof/transfer_oof_predictions.jsonl
```

## Validation boundary

Do not change thresholds or approved keys after independent validation data is
opened. If any candidate setting is changed, rebuild the candidate and treat the
next external corpus as a new independent validation set.

The next stage must score a genuinely independent labelled export against this
frozen artifact. At minimum it should reject function/family overlap with the
development corpus, require sufficient support for every enabled candidate key,
verify per-key promotion precision, compare fused precision against the unchanged
base v4 accepts, and emit a separate validation result. Only that result can
justify a later production-router change.
