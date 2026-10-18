# Manual recovery candidate retrieval

Use the recovered functions as similarity references to find unlabeled native
functions worth inspecting next. This produces candidates and their nearest
gold examples; it does not assign labels or change classifier routing.

## Current labeled data

The local `dataset-manual-restoration-v68` was built from the shared portable
P-code corpus, the v65 aggregate, and the three tracked follow-up label files.
It contains 9,249 labelled functions, 9,906 label rows, and 6,955 structural
groups; family mining produced 4,643 families, 758 with multiple members. All
three new functions were surfaced by advisory retrieval and confirmed from
their original Thumb bodies against gold-labelled parallels.

The v68 query searched 29,553 native functions, retained 28,826 with portable
P-code, skipped 726 already-labelled functions, and indexed 108 families with
function-role labels. The top-100 candidates had mean nearest-family
similarity `0.6392`, maximum `1.0`, and mean top-two gap `0.0381`. A new
five-function `PlayerStatusInfo::add_Add*` cluster is queued for review: its
functions share the same gated float-accumulation structure, but their
downstream meaning still needs call-site evidence before adding role labels.

## Build the index

```bash
NGML="$HOME/.local/share/nevergone-ml-v2"
PYTHON="${PYTHON:-$HOME/.venvs/nevergone-ml/bin/python}"
cat "$NGML/labels/labels-with-manual-restorations-v65.jsonl" \
  docs/evidence/manual-nevergone-recoveries/lansquenet-move-state-labels.jsonl \
  docs/evidence/manual-nevergone-recoveries/lansquenet-death-labels.jsonl \
  docs/evidence/manual-nevergone-recoveries/pet-death-labels.jsonl \
  > "$NGML/labels/labels-with-manual-restorations-v68.jsonl"
"$PYTHON" tools/ml/build_nevergone_dataset.py \
  "$NGML/corpus/corpus.jsonl" \
  "$NGML/labels/labels-with-manual-restorations-v68.jsonl" \
  "$NGML/dataset-manual-restoration-v68"
"$PYTHON" tools/ml/mine_nevergone_function_families.py \
  "$NGML/dataset-manual-restoration-v68" \
  "$NGML/families-manual-restoration-v68.jsonl"
"$PYTHON" tools/ml/build_nevergone_similarity_index.py \
  "$NGML/dataset-manual-restoration-v68" \
  "$NGML/families-manual-restoration-v68.jsonl" \
  "$NGML/manual-recovery-index-v68"
```

The index keeps function names and addresses as display metadata only. The
portable P-code remains the similarity input.

## Rank unlabeled functions

```bash
"$PYTHON" tools/ml/search_nevergone_function_families.py \
  "$NGML/manual-recovery-index-v68" \
  "$NGML/corpus/libcocos2dcpp.jsonl" \
  --output "$NGML/manual-recovery-candidates-v68.jsonl" \
  --top-k 5 \
  --label-task function_role \
  --families "$NGML/families-manual-restoration-v68.jsonl" \
  --unlabelled-only \
  --top-candidates 100
```

Only families with `function_role` labels act as references. Query functions
whose own family already has such a label are omitted. The output is sorted by
top cosine similarity and then by the top-two gap; each candidate includes the
nearest labelled family, its role-label distribution, and trace examples.

Similarity identifies structural resemblance, not semantic equivalence. Review
the displayed gold evidence and confirm the function from its own callers,
callees, data flow, and binary before adding another label.
