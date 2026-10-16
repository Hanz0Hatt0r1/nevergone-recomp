# revenginstr migration

Generic reverse-engineering mechanisms developed in Nevergone are being extracted
to `Hanz0Hatt0r1/revenginstr`. Nevergone remains the reference/training project
and keeps all Cocos/game-specific labels, class/vtable maps, datasets and semantic
evidence.

## Already genericized

The preferred external core now owns:

- architecture-normalized portable high-P-code export/audit;
- cross-project monotone OOD guard;
- callgraph metrics;
- mass receiver-plan construction;
- bounded indirect-call receiver/type acquisition;
- receiver class/vtable identity matching from conservative type evidence and a consumer-owned catalog;
- leakage-safe numeric evidence aggregation across callgraph/receiver sources;
- deterministic structural P-code metrics and coarse numeric evidence tokenization;
- portable function-family mining with a Nevergone-compatible hash profile;
- family-grouped P-code selective classification as `REVENGINSTR.SelectiveClassifier/1`;
- evidence-aware selective classification as `REVENGINSTR.EvidenceSelectiveClassifier/1`;
- family-grouped accept-only correctness routing as `REVENGINSTR.AcceptErrorRouter/1`.

`run_nevergone_production_classifier_v51.sh` auto-detects a sibling
`../revenginstr` clone or accepts:

```bash
export REVENGINSTR_REPO=/path/to/revenginstr
```

When available it delegates callgraph extraction, receiver-plan creation, mass
receiver identity inference and feature aggregation to the generic package.
`REVENGINSTR.ReceiverInference/1` and `REVENGINSTR.Evidence/1` are converted by
thin Nevergone compatibility adapters to the historical v5 format identifiers.
The feature dictionary itself is not renamed or reinterpreted.

Historical Nevergone algorithms/artifacts remain immutable reference lineage until
retained output equivalence is frozen.

## P-code-only selective-classifier parity

Historical `NEVERGONE.ProductionClassifier/4` remains immutable reference data.
The generic implementation is validated by retraining it on the same frozen
Nevergone dataset/family map and comparing family-held-out OOF rows:

```bash
bash tools/ml/run_nevergone_revenginstr_selective_parity.sh \
  ~/.local/share/nevergone-ml-v2
```

Outputs:

```text
~/.local/share/nevergone-ml-v2/models/revenginstr-selective-v1/
~/.local/share/nevergone-ml-v2/evidence-revenginstr-selective/parity.json
```

## Evidence-aware v5.2 parity

Historical `production-code-domain-v5.2` remains the evidence-aware reference.
The consumer adapter rewrites only the outer evidence format identifier from
`NEVERGONE.ProductionEvidence/1` to `REVENGINSTR.Evidence/1`; ids, feature keys and
numeric values are preserved exactly. The generic classifier is then trained on
the same frozen dataset/families/evidence:

```bash
bash tools/ml/run_nevergone_revenginstr_evidence_parity.sh \
  ~/.local/share/nevergone-ml-v2
```

Outputs:

```text
~/.local/share/nevergone-ml-v2/evidence-revenginstr-v5.2/evidence.jsonl
~/.local/share/nevergone-ml-v2/models/revenginstr-evidence-selective-v1/
~/.local/share/nevergone-ml-v2/evidence-revenginstr-selective/evidence-v5.2-parity.json
```

Both parity bridges use the same strict audit: identical row population,
predictions and routing actions plus numerically equivalent raw/calibrated
confidence, family similarity, competing similarity, contrast margin and
calibration/selective metrics. Downstream consumers should switch only after the
retained local checks pass.

## Still Nevergone-specific

The following remain in this repository because they are consumer data, semantic
knowledge or experiment lineage rather than reusable mechanism:

- seed-label rules and Cocos class identities;
- current vtable-slot export/catalog data and Nevergone catalog format;
- thin compatibility adapters for historical receiver/evidence format IDs;
- production experiment/evaluation lineage (v5.x through v10), including the
  exact nested policies and retained OOF result files that justified promotion;
- frozen Nevergone datasets and validation evidence;
- Nevergone-specific weak/gold labels and ground truth.

The generic mechanisms for evidence-aware classification and accept-error
production packaging now live in `revenginstr`; the remaining migration work is
consumer parity, wrapper replacement and deletion of duplicated fallback
implementations after equivalence is proven.
