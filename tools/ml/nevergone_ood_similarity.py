#!/usr/bin/env python3
"""Portable-P-code TF-IDF family-prototype similarity helpers.

The helpers are intentionally model-agnostic. They use the production
classifier's fitted TF-IDF representation and compare a sample against
family-level centroids from the selected training corpus.
"""
from __future__ import annotations

from collections import defaultdict
from typing import Iterable


def build_family_prototypes(vectorizer, texts: list[str], labels: list[str], families: list[str]):
    """Return L2-normalized sparse family centroids and family metadata."""
    import numpy as np
    from scipy import sparse
    from sklearn.preprocessing import normalize

    matrix = vectorizer.transform(texts)
    members: dict[str, list[int]] = defaultdict(list)
    for index, family_id in enumerate(families):
        members[family_id].append(index)

    prototype_rows = []
    prototype_labels: list[str] = []
    prototype_families: list[str] = []

    for family_id in sorted(members):
        idx = members[family_id]
        family_labels = {labels[i] for i in idx}
        if len(family_labels) != 1:
            continue
        centroid = matrix[idx].mean(axis=0)
        centroid = sparse.csr_matrix(centroid)
        centroid = normalize(centroid, norm="l2", copy=False)
        prototype_rows.append(centroid)
        prototype_labels.append(next(iter(family_labels)))
        prototype_families.append(family_id)

    if prototype_rows:
        prototypes = sparse.vstack(prototype_rows, format="csr")
    else:
        prototypes = sparse.csr_matrix((0, matrix.shape[1]), dtype=np.float64)

    return {
        "matrix": prototypes,
        "labels": prototype_labels,
        "families": prototype_families,
    }


def max_similarity_by_predicted_class(
    vectorizer,
    prototypes,
    texts: Iterable[str],
    predicted_labels: Iterable[str],
) -> list[float]:
    """Maximum cosine similarity to a family prototype of the predicted class."""
    texts = list(texts)
    predicted_labels = list(predicted_labels)
    if len(texts) != len(predicted_labels):
        raise ValueError("text/prediction length mismatch")

    matrix = vectorizer.transform(texts)
    proto_matrix = prototypes["matrix"]
    proto_labels = list(prototypes["labels"])

    class_indices: dict[str, list[int]] = defaultdict(list)
    for index, label in enumerate(proto_labels):
        class_indices[label].append(index)

    result: list[float] = []
    for row_index, label in enumerate(predicted_labels):
        indices = class_indices.get(label)
        if not indices:
            result.append(0.0)
            continue
        similarities = matrix[row_index].dot(proto_matrix[indices].T)
        if similarities.nnz:
            result.append(float(similarities.max()))
        else:
            result.append(0.0)
    return result
