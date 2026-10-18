# Frozen unseen500 v10 receiver coverage

This report measures receiver-query availability and unique-vtable resolution
on the 500-function `unseen500-v10` identity-disjoint holdout. The
path-scrubbed `source-manifest.json` records the original manifest SHA-256
`6d941481b80aa30feef1e302fecc855f28f4d27fd5efb74aa2ddc618e504e856` and pins
the source corpus to SHA-256
`c4ac5b71a5ca89e3d18490141892b59a7b71d53920290eca9ce09c712ba69c94`; the
copied `source-validation-summary.json` confirms the holdout has no semantic
labels.

## Results

| Measure | Count | Coverage |
| --- | ---: | ---: |
| Frozen functions with at least one receiver query | 100 / 500 | 20.0% |
| Queries resolved to one unique vtable | 66 / 203 | 32.5% |
| Frozen functions with at least one unique-vtable query | 53 / 500 | 10.6% |

Query statuses: 66 `resolved_unique_vtable`, 76 `unresolved_receiver_class`,
and 61 `receiver_not_single_pointer`. None of the 203 rows has dispatch-slot
evidence or a runtime validation result.

These are availability and resolution measurements only. The frozen holdout
has no independent labels, so the report makes no semantic precision or
accuracy claim and does not evaluate exact-evidence mismatch gates. The M2.6
35% useful-coverage target is not demonstrated by these results; query-level
resolution is below that threshold, and function-level resolved coverage is
10.6%.

## Inputs and reproduction

`function-population.jsonl` contains the 500 frozen function identities copied
from the source corpus. `receiver-vtable-inference.jsonl` is the frozen
receiver inference output. Their SHA-256 values and row counts are pinned in
`benchmark-manifest.json` and repeated in `benchmark-summary.json`. The report
deduplicates functions by `source_id`, while query coverage retains one row per
indirect-call query.

With `revenginstr` at the manifest's pinned source revision installed, rerun
the aggregate report from this directory:

```bash
revenginstr-benchmark benchmark-manifest.json --mode smoke --output benchmark-summary.json
```

The projection rejects duplicate population identities and queries outside
the frozen population. It writes only aggregate counts and fractions; function
identities remain in these Nevergone-owned input artifacts.
