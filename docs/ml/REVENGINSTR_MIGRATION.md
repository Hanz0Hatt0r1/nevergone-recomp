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
- leakage-safe numeric evidence aggregation across callgraph/receiver sources.

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

## Still Nevergone-specific

The following remain in this repository because they are consumer data, semantic
knowledge or experiment lineage rather than reusable mechanism:

- seed-label rules and Cocos class identities;
- current vtable-slot export/catalog data and Nevergone catalog format;
- thin compatibility adapters for historical receiver/evidence format IDs;
- production classifier experiment lineage (v4/v5.x/v9/v10);
- frozen Nevergone datasets and validation evidence.

The next extraction boundaries are family/grouped validation utilities and generic
production model packaging. The Cocos/FFmpeg labels and Nevergone ground truth stay
here.
