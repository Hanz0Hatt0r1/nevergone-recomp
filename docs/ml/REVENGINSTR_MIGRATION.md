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
- bounded indirect-call receiver/type acquisition.

`run_nevergone_production_classifier_v51.sh` auto-detects a sibling
`../revenginstr` clone or accepts:

```bash
export REVENGINSTR_REPO=/path/to/revenginstr
```

When available it delegates callgraph extraction and receiver-plan creation to the
generic package. Historical Nevergone scripts remain compatibility fallbacks while
retained outputs are compared.

## Still Nevergone-specific

The following remain in this repository until their project assumptions are
removed:

- seed-label rules and Cocos class identities;
- static vtable catalog interpretation tied to current Nevergone formats;
- class/vtable receiver inference compatibility layer;
- production classifier experiment lineage (v4/v5.x/v9/v10);
- frozen Nevergone datasets and validation evidence.

The next extraction boundary is receiver class/vtable inference, followed by
family/grouped evaluation and generic production model packaging.
