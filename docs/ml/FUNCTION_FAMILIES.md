# Nevergone Function Family Miner

The exact-structure split in `MlDataset/2` prevents byte-identical normalized
functions from crossing split boundaries. It does not prevent near-duplicate
wrappers, constructors, accessors or container helpers from appearing in both
training and evaluation data.

`tools/ml/mine_nevergone_function_families.py` preserves the Nevergone command
and output format while delegating family mining to the generic
`revenginstr.family` package. It uses only the portable P-code view: operation
ordinals, exact SSA identities, exact small offsets and model headers are
excluded. Function names, source addresses and labels are never used to decide
family membership.

## Representation

The implementation lives in `revenginstr.family.features`.
`tools/ml/nevergone_pcode_features.py` remains as a compatibility adapter for
existing Nevergone training scripts. It supplies Nevergone's historical MinHash
seed so existing family IDs stay stable:

- `o123` becomes `OP`;
- `v123:4` becomes `v:4`;
- exact small constants become magnitude buckets;
- opcode order, operand roles and widths remain;
- internal/external/indirect call kinds remain;
- architecture/calling-convention/size headers are excluded from family mining.

Family features include normalized operation shapes, opcode/call-kind counts and
2/3/4-operation sequence shingles.

## Mining families

Candidate near-duplicate pairs are found with deterministic 32-value MinHash and
8-band LSH. Candidates are then checked with exact Jaccard similarity over the
family feature sets. Exact portable duplicates are always grouped.

Default policy:

```text
Jaccard threshold: 0.72
minimum emitted-op length ratio: 0.55
MinHash values: 32
LSH bands: 8
```

Run on an existing dataset:

```bash
python3 tools/ml/mine_nevergone_function_families.py \
  /tmp/nevergone-ml-v2/dataset-4class \
  /tmp/nevergone-ml-v2/families.jsonl
```

The command also writes:

```text
/tmp/nevergone-ml-v2/families.jsonl.manifest.json
```

The manifest reports family count, multi-member families, largest family,
candidate/accepted pair counts, and how many mined families contain conflicting
labels for each task. Mixed-label families are not silently split; keeping them
together is part of the leakage-resistant evaluation.

## Family-grouped cross-validation

The CV driver requires NumPy and scikit-learn, but the miner itself uses only the
Python standard library.

```bash
source /tmp/nevergone-ml-venv/bin/activate
python tools/ml/nevergone_family_grouped_cv.py \
  /tmp/nevergone-ml-v2/dataset-4class \
  /tmp/nevergone-ml-v2/families.jsonl \
  --classes game engine runtime_library media_library
```

`StratifiedGroupKFold` receives `family_id` as the group key, so every member of
a mined near-duplicate family is confined to one fold. The classifier consumes
only portable P-code.

Install `revenginstr` in the active Python environment, or set
`REVENGINSTR_REPO` to a checkout of that repository. The default local layout
also works when both repositories are sibling directories.

The family miner is a conservative structural heuristic, not semantic proof that
two functions implement the same source-level operation. Its purpose is to make
held-out accuracy harder to inflate with near-duplicate implementation patterns.
Threshold sensitivity should be reported before treating a score as a final
cross-family estimate.
