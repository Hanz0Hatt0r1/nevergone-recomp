# Nested similarity-transfer validation

`nevergone_similarity_transfer_cv.py` shows whether nearest known families carry useful labels, but its threshold grid is exploratory: selecting thresholds from that same evaluation pool and quoting their precision would be optimistic.

`nevergone_nested_similarity_transfer_cv.py` adds an outer/inner family-grouped protocol. For each outer fold, inner family-held-out retrieval chooses class-specific `top_similarity` + `top_gap` policies. Those policies are then applied to untouched outer families.

The evaluator uses portable P-code only. Function names, addresses, binary names, strings and project goals are not retrieval inputs.

Example for code-domain evidence:

```bash
python3 tools/ml/nevergone_nested_similarity_transfer_cv.py \
  /tmp/nevergone-ml-v2/dataset-4class-internal \
  /tmp/nevergone-ml-v2/families-internal.jsonl \
  --task code_domain \
  --classes game engine runtime_library media_library \
  --outer-folds 5 \
  --inner-folds 4 \
  --target-precision 0.95 \
  --min-policy-examples 10
```

A predicted class receives no transfer accept policy in an outer fold unless its inner OOF retrieval has at least the requested number of examples meeting the target precision. The final `nested selective` line is therefore an outer-held-out estimate of transfer-evidence precision and coverage.

This evaluator does not modify production routing. A production evidence channel should only be added after nested precision is acceptable and class-level policy stability is understood.
