# Production Classifier v5.1: mass receiver/type evidence

`ProductionClassifier/5.1` is an experiment layer on top of the existing
`NEVERGONE.ProductionClassifier/5` artifact format. It does not introduce a new
serialized classifier contract; it changes how receiver/type evidence is collected
before training.

The first v5 run on the persistent 9,118-function dataset produced:

```text
v4 balanced=0.8160 macro_f1=0.7370 accept_precision=0.9661 accept_coverage=0.6023
v5 balanced=0.8281 macro_f1=0.7462 accept_precision=0.9664 accept_coverage=0.5741
```

That run also exposed an important coverage failure:

```text
receiver_inference: unmatched=62
receiver_types:     unmatched=62
```

The 62 observations came from the bounded 19-function receiver frontier and did not
overlap the current 9,118-function training set. Therefore the initial v5 improvement
was effectively structural/callgraph improvement, not evidence that receiver/type
inference had helped the classifier.

## v5.1 objective

v5.1 scales receiver/type collection to every `libcocos2dcpp.so` function represented
in the current dataset trace. The existing Ghidra exporter already accepts an arbitrary
TSV plan, so no second receiver exporter is needed.

Pipeline:

```text
dataset trace
    |
    +--> mass receiver plan (all libcocos2dcpp.so training functions)
    |
    +--> Ghidra ExportIndirectCallReceiverTypes.java
    |       discovers every CALLIND in the selected functions
    |
    +--> ExportVtableSlots.java -> static vtable catalog
    |
    +--> mass receiver identity inference
    |       direct one-level pointer type
    |       transparent COPY/CAST/INDIRECT/MULTIEQUAL type flow
    |       unique catalog class/vtable identity
    |       no dispatch-slot/target transfer
    |
    +--> per-source coverage audit
    |
    +--> existing v5 evidence aggregation
    |
    +--> existing ProductionClassifier/5 trainer
```

The mass inference intentionally resolves only receiver identity facts that are valid
without a CALLIND target slice. It does **not** infer a virtual method target or claim a
dispatch slot. Intermediate evidence may contain Ghidra class/vtable identities, but
`build_nevergone_v5_evidence.py` reduces them to coarse counts/status/source features;
raw class names, vtable IDs and addresses are not model tokens.

## Run

Use the persistent ML root created for the current training corpus:

```bash
cd ~/nevergone-recomp
git switch main
git pull --ff-only
source ~/.venvs/nevergone-ml/bin/activate

bash tools/ml/run_nevergone_production_classifier_v51.sh \
  ~/.local/share/nevergone-ml-v2 \
  | tee ~/.local/share/nevergone-ml-v2/logs/v51-full-run.txt
```

The runner uses these defaults, all overrideable through environment variables:

```text
GHIDRA_HOME=/opt/ghidra
GHIDRA_PROJECT_DIR=/home/pes/ghidra_projects/nevergone
GHIDRA_PROJECT_NAME=nevergone
GHIDRA_PROGRAM=libcocos2dcpp.so
```

Persistent outputs:

```text
~/.local/share/nevergone-ml-v2/receiver-v5.1/receiver-plan.tsv
~/.local/share/nevergone-ml-v2/receiver-v5.1/indirect-receiver-types.tsv
~/.local/share/nevergone-ml-v2/receiver-v5.1/vtable-slots.tsv.gz
~/.local/share/nevergone-ml-v2/receiver-v5.1/static-vtable-slot-catalog.jsonl
~/.local/share/nevergone-ml-v2/receiver-v5.1/static-receiver-vtable-inference-mass.jsonl
~/.local/share/nevergone-ml-v2/evidence-v5.1/evidence.jsonl
~/.local/share/nevergone-ml-v2/models/production-code-domain-v5.1/
```

## Coverage semantics

The v5.1 runner emits a dedicated coverage audit before training. This is separate from
`build_nevergone_v5_evidence.py`'s aggregate coverage because callgraph metrics cover
nearly all labelled functions and can otherwise hide missing receiver evidence.

The audit reports independently:

- dataset functions and per-program distribution;
- `libcocos2dcpp.so` functions eligible for receiver export;
- callgraph matched-function coverage;
- receiver-type CALLIND rows and matched-function coverage;
- receiver-inference matched-function coverage;
- number of functions with at least one uniquely resolved receiver vtable;
- receiver inference status counts.

A function with no indirect calls is not expected to have a receiver row. Therefore
receiver function coverage is not itself a success-rate metric; the more useful counts
are total CALLIND rows, inference status distribution, and how many labelled functions
receive at least one receiver-derived feature.

## Interpretation

v5.1 should be compared to both v4 and the first v5 run on identical family-grouped
folds. The primary questions are:

1. Does mass receiver evidence improve balanced accuracy and macro-F1 beyond v5?
2. Does `game` precision continue to improve without sacrificing `runtime_library`?
3. Can accept coverage recover toward or above the v4 0.6023 level while preserving
   approximately 0.97 accept precision?
4. Does receiver evidence help on family-held-out functions rather than merely memorize
   recovered class identities? Raw class names are excluded specifically to protect this
   test.

The first practical target remains:

```text
balanced_accuracy > 0.835
macro_f1          > 0.755
accept_precision  >= 0.97
accept_coverage   >= 0.60
game precision    > 0.40
```

Failure to reach those numbers is still useful: the coverage/status audit will show
whether the issue is insufficient receiver evidence, unresolved receiver identity, or
simply weak predictive value for the code-domain task.
