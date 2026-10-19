# Manual recovery candidate retrieval

Use recovered functions as similarity references to find unlabeled native
functions worth inspecting. Retrieval suggests candidates; it does not assign
labels or change classifier routing.

## Current labeled data

The local `dataset-manual-restoration-v70` contains 9,301 labelled functions,
10,010 label rows, and 6,997 structural groups. Family mining produced 4,665
families, 759 with multiple members. The latest manual batch adds 12 gold
functions covering hit/combo-window gates, counter advances, and counter reads
for `EnemyObject`, `LansquenetObject`, and `ShootObject`.

The v70 query examined 29,553 native functions, retained 28,703 with portable
P-code, skipped 849 already-labelled functions, and skipped one function
without P-code. It used 136 function-role reference families. Among the top
100 candidates, nearest-family similarity ranged from `0.0806` to `1.0`, with
mean `0.6603` and mean top-two gap `0.0368`.

Manual label files live in
`docs/evidence/manual-nevergone-recoveries/*labels.jsonl`. Do not concatenate a
hand-picked subset: some files overlap the local aggregate, and some task
groups contain conflicting hypotheses. The merger canonicalizes identities
against the corpus, keeps existing base labels on conflict, skips conflicting
supplemental groups, and prints a report.

## Build the index

```bash
NGML="$HOME/.local/share/nevergone-ml-v2"
PYTHON="${PYTHON:-$HOME/.venvs/nevergone-ml/bin/python}"
"$PYTHON" tools/ml/merge_nevergone_manual_labels.py \
  "$NGML/corpus/corpus.jsonl" \
  "$NGML/labels/labels-with-manual-restorations-v69.jsonl" \
  docs/evidence/manual-nevergone-recoveries \
  "$NGML/labels/labels-with-manual-restorations-v70.jsonl"
"$PYTHON" tools/ml/build_nevergone_dataset.py \
  "$NGML/corpus/corpus.jsonl" \
  "$NGML/labels/labels-with-manual-restorations-v70.jsonl" \
  "$NGML/dataset-manual-restoration-v70"
"$PYTHON" tools/ml/mine_nevergone_function_families.py \
  "$NGML/dataset-manual-restoration-v70" \
  "$NGML/families-manual-restoration-v70.jsonl"
"$PYTHON" tools/ml/build_nevergone_similarity_index.py \
  "$NGML/dataset-manual-restoration-v70" \
  "$NGML/families-manual-restoration-v70.jsonl" \
  "$NGML/manual-recovery-index-v70"
```

The index uses portable P-code. Names and addresses remain display metadata.

## Rank unlabeled functions

```bash
"$PYTHON" tools/ml/search_nevergone_function_families.py \
  "$NGML/manual-recovery-index-v70" \
  "$NGML/corpus/libcocos2dcpp.jsonl" \
  --output "$NGML/manual-recovery-candidates-v70.jsonl" \
  --top-k 5 \
  --label-task function_role \
  --families "$NGML/families-manual-restoration-v70.jsonl" \
  --unlabelled-only \
  --top-candidates 100
```

Only families with `function_role` labels act as references. Query functions
whose own family already has such a label are omitted. Each result includes the
nearest labelled family, its role-label distribution, and trace examples.

Similarity identifies structural resemblance, not semantic equivalence. Check
the gold evidence, call sites, callees, data flow, and original binary before
adding a label or changing game behavior.
