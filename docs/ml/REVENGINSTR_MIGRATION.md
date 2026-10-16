# revenginstr migration

Generic reverse-engineering mechanisms developed in Nevergone are being extracted
to `Hanz0Hatt0r1/revenginstr`. Nevergone remains the reference/training project
and keeps all Cocos/game-specific labels, class/vtable maps, datasets and semantic
evidence.

## Already genericized

The preferred external core now owns:

- architecture-normalized portable high-P-code export/audit;
- read-only ARM32 Itanium primary-vtable slot export;
- cross-project monotone OOD guard;
- callgraph metrics;
- mass receiver-plan construction;
- bounded indirect-call receiver/type acquisition;
- bounded direct-caller argument-subtree export;
- receiver class/vtable identity matching from conservative type evidence and a consumer-owned catalog;
- leakage-safe numeric evidence aggregation across callgraph/receiver sources;
- deterministic structural P-code metrics and coarse numeric evidence tokenization;
- portable function-family mining with consumer-owned compatibility hash profiles;
- family-grouped P-code selective classification as `REVENGINSTR.SelectiveClassifier/1`;
- evidence-aware selective classification as `REVENGINSTR.EvidenceSelectiveClassifier/1`;
- family-grouped accept-only correctness routing as `REVENGINSTR.AcceptErrorRouter/1`;
- deterministic identity-disjoint frozen validation as `REVENGINSTR.FrozenValidation/1`;
- unlabeled deployment-distribution diagnostics as `REVENGINSTR.DriftAudit/1`.

`run_nevergone_production_classifier_v51.sh` auto-detects a sibling
`../revenginstr` clone or accepts:

```bash
export REVENGINSTR_REPO=/path/to/revenginstr
```

When available it delegates callgraph extraction, receiver-plan creation,
indirect receiver/type export, ARM32 Itanium vtable-slot export, receiver identity
inference and feature aggregation to the generic package. The generic Ghidra
exporters and inference consume a Nevergone-owned vtable catalog; no class map,
address list or labels move into `revenginstr`. Thin adapters preserve the
historical `NEVERGONE.*` receiver TSV and inference identifiers for audit and
compatibility consumers, while shared aggregation reads generic artifacts
directly. The feature dictionary itself is not renamed or reinterpreted.

`run_nevergone_frozen_unseen500_v10.sh` uses the shared receiver and ARM32 Itanium
vtable exporters when the sibling checkout is available, plus shared receiver
inference. Its immutable corpus, receiver plan, vtable catalog and evidence remain
Nevergone-owned. The legacy Java exporters and inference remain available as an
offline fallback.

Before switching the runner, the shared exporter was run in read-only mode over
the retained 500-function validation plan. Its 203 indirect-call rows matched the
local exporter field-for-field after adapting only the outer format identifier.
The generic inference adapter likewise reproduced all 203 retained Nevergone
inference rows exactly, including nested receiver-type format identifiers. These
checks establish output compatibility for that corpus; they do not transfer its
game-specific receiver or vtable knowledge into the shared package.

The shared ARM32 Itanium vtable exporter also reproduced all 56,340 retained
Nevergone pointer rows byte-for-byte after decompression. Its scanner is generic;
the symbol names, addresses and interpreted catalog stay in Nevergone.

Historical Nevergone algorithms/artifacts remain immutable reference lineage until
retained output equivalence is frozen.

The Nevergone callgraph, mass receiver-plan and structural-metrics command names
now prefer the corresponding `revenginstr` implementations when the shared
checkout is available. Their adapters preserve the historical Nevergone output
formats where downstream tools still expect them; the callgraph and plan tools
keep their offline compatibility paths.

The bounded caller-argument SSA walk now uses the generic Ghidra exporter when
the shared checkout is available. Nevergone keeps the source plan, coverage
expectations, format identifier and TSV shape in its adapter; the previous local
exporter remains the offline fallback.

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

Both classifier parity bridges use the same strict audit: identical row population,
predictions and routing actions plus numerically equivalent raw/calibrated
confidence, family similarity, competing similarity, contrast margin and
calibration/selective metrics. Downstream consumers should switch only after the
retained local checks pass.

## Accept-router production gate

The promoted Nevergone v10 router remains retained reference lineage. The generic
`REVENGINSTR.AcceptErrorRouter/1` is trained from the generic v5.2-equivalent OOF
and evidence and compared against the retained production requirements:

```bash
bash tools/ml/run_nevergone_revenginstr_accept_router_gate.sh \
  ~/.local/share/nevergone-ml-v2
```

The migration gate remains fail-closed at the production constraints used by the
Nevergone promotion decision; generic packaging does not silently relax precision
or coverage to obtain parity.

## Frozen validation and drift parity

Frozen-set **mechanism** is now generic, while Nevergone retains the actual frozen
bytes, labels/absence of labels, origin policy and experiment history.

Nevergone's historical corpus format is adapted by changing only the outer P-code
format identifier to `REVENGINSTR.PcodeCorpus/1`. New frozen exports delegate to
the shared Ghidra exporter and already use that format; the adapter accepts both
versions. Nevergone itself emits an eligibility trace using
`nevergone_function_origin.py`, so its narrow `_INIT/_FINI` runtime-scaffold policy
does not leak into the common validation package.

A new generic frozen generation is intentionally distinct from the old v10 set:

```bash
bash tools/ml/run_nevergone_revenginstr_validation_parity.sh \
  ~/.local/share/nevergone-ml-v2
```

The runner:

1. adapts the retained wide Nevergone P-code corpus to the generic outer contract;
2. writes a Nevergone-owned semantic-model eligibility trace;
3. excludes the current labelled dataset trace;
4. freezes a **new** deterministic `revenginstr-unseen500-v1` population with a
   dedicated selection salt and immutable hashes;
5. converts retained v5.2/frozen evidence to `REVENGINSTR.Evidence/1` without
   changing feature values;
6. reduces historical v9 routing lineage to generic `{id, action}` baseline rows;
7. re-runs `REVENGINSTR.DriftAudit/1` on the unchanged historical frozen-v10
   classified/evidence files;
8. requires strict numeric parity with the retained
   `NEVERGONE.FrozenDriftAuditV10/1` report.

Outputs:

```text
~/.local/share/nevergone-ml-v2/frozen-validation/revenginstr-unseen500-v1/
  inputs/
  frozen/
    corpus.jsonl
    trace.jsonl
    manifest.json
  historical-drift.revenginstr.json
  historical-drift-parity.json
```

The new frozen generation is not presented as a byte-for-byte reconstruction of
the historical v10 sample. The historical sample remains immutable and is used only
for the migration parity gate. Future evidence/classification runs should target the
new generic frozen population without tuning thresholds against it.

## Still Nevergone-specific

The following remain in this repository because they are consumer data, semantic
knowledge or experiment lineage rather than reusable mechanism:

- seed-label rules and Cocos class identities;
- current vtable-slot catalog data and Nevergone catalog format;
- Nevergone function-origin/runtime-scaffold eligibility policy;
- thin compatibility adapters for historical receiver/evidence/P-code format IDs;
- production experiment/evaluation lineage (v5.x through v10), including the
  exact nested policies and retained OOF result files that justified promotion;
- frozen Nevergone datasets and validation evidence bytes;
- Nevergone-specific weak/gold labels and ground truth.

The reusable algorithms for evidence-aware classification, accept-error routing,
frozen holdout selection and deployment drift now live in `revenginstr`; remaining
work is consumer wrapper replacement and deletion of duplicated historical fallback
implementations after retained equivalence is proven.
