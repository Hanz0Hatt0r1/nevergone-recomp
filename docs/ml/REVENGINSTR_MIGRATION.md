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
- portable function-family mining with a Nevergone-compatible hash profile;
- family-grouped P-code selective classification, correctness calibration and contrastive family OOD routing as `REVENGINSTR.SelectiveClassifier/1`.

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

Historical Nevergone algorithms remain temporary fallback paths until retained
output equivalence is frozen.

## Selective-classifier parity bridge

Historical `NEVERGONE.ProductionClassifier/4` remains immutable reference data.
The generic implementation is validated by retraining it on the same frozen
Nevergone dataset/family map and comparing family-held-out OOF rows:

```bash
bash tools/ml/run_nevergone_revenginstr_selective_parity.sh \
  ~/.local/share/nevergone-ml-v2
```

The runner writes:

```text
~/.local/share/nevergone-ml-v2/models/revenginstr-selective-v1/
~/.local/share/nevergone-ml-v2/evidence-revenginstr-selective/parity.json
```

`--strict` parity requires identical row population, predictions and routing
actions plus numerically equivalent raw/calibrated confidence, family similarity,
competing similarity, contrast margin and top-level calibration/selective metrics.
Only after this retained check passes should downstream consumers replace the
historical Nevergone v4 classifier CLI with the generic artifact.

## Still Nevergone-specific

The following remain in this repository because they are consumer data, semantic
knowledge or experiment lineage rather than reusable mechanism:

- seed-label rules and Cocos class identities;
- current vtable-slot export/catalog data and Nevergone catalog format;
- thin compatibility adapters for historical receiver/evidence format IDs;
- production experiment lineage beyond the generic P-code-only selective baseline (v5.x/v9/v10);
- evidence-aware feature augmentation and accept-error-router packaging until their generic contracts are extracted;
- frozen Nevergone datasets and validation evidence.

The next extraction boundaries are evidence-aware classifier augmentation and the
production accept-error router. The Cocos/FFmpeg labels and Nevergone ground truth
stay here.
