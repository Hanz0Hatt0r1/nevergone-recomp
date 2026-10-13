# Semantic family similarity search

This stage adds retrieval over the existing portable-P-code corpus. It does not create a new classifier and does not change production routing.

## Representation

The index uses the same portable P-code normalization as the validated classifier baseline. TF-IDF uses whitespace tokenization, unigram/bigram features, `min_df=2`, `max_features=80000`, and sublinear term frequency.

Each known function is mapped to its mined function family. The index stores one L2-normalized TF-IDF centroid per family. Query similarity is cosine similarity between the query TF-IDF vector and each family centroid.

Function names, source addresses, binary names, strings, raw decompiler C, and project goals are not similarity inputs. `trace.jsonl` names/addresses may appear only as display metadata in returned family examples.

## Build

```bash
python3 tools/ml/build_nevergone_similarity_index.py \
  /tmp/nevergone-ml-v2/dataset-4class-internal \
  /tmp/nevergone-ml-v2/families-internal.jsonl \
  /tmp/nevergone-ml-v2/similarity-index
```

The output directory contains `index.joblib` and `manifest.json`.

## Search

```bash
python3 tools/ml/search_nevergone_function_families.py \
  /tmp/nevergone-ml-v2/similarity-index \
  /tmp/nevergone-internal-unseen-500.jsonl \
  --output /tmp/nevergone-internal-unseen-500.similarity.jsonl \
  --top-k 5
```

Each result contains:

- `top_similarity`: cosine similarity to the nearest known family.
- `top_gap`: top-1 minus top-2 similarity.
- `semantic_novelty`: `1 - top_similarity`, clipped to `[0,1]`.
- `neighbors`: ranked known family IDs with family size, known label distributions, and optional trace examples.

These are descriptive retrieval signals only. There is deliberately no `accept`, `review`, or project-goal-dependent priority attached to similarity output.
