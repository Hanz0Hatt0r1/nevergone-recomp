#!/usr/bin/env python3
"""Portable-P-code TF-IDF family-prototype similarity helpers.

The helpers are intentionally model-agnostic. They use a fitted TF-IDF
representation and compare a sample against family-level centroids from the
selected training corpus. No names, addresses, binary identity, or strings are
used.
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
            # A mixed-label family is unsafe as a semantic reference prototype.
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


def contrastive_family_scores(
    vectorizer,
    prototypes,
    texts: Iterable[str],
    predicted_labels: Iterable[str],
) -> list[dict[str, object]]:
    """Compare nearest predicted-class family against the strongest competitor.

    Returns one record per input with:
      predicted_similarity: nearest family cosine for the predicted class
      predicted_family: that family's stable family ID
      competing_similarity: nearest family cosine from any other class
      competing_family: nearest competing family ID
      competing_label: competing family's class
      contrast_margin: predicted_similarity - competing_similarity

    A positive margin means the structural nearest-neighbour evidence supports
    the classifier's predicted class more strongly than any competing class.
    """
    texts = list(texts)
    predicted_labels = list(predicted_labels)
    if len(texts) != len(predicted_labels):
        raise ValueError("text/prediction length mismatch")

    matrix = vectorizer.transform(texts)
    proto_matrix = prototypes["matrix"]
    proto_labels = list(prototypes["labels"])
    proto_families = list(prototypes["families"])

    class_indices: dict[str, list[int]] = defaultdict(list)
    for index, label in enumerate(proto_labels):
        class_indices[label].append(index)

    result: list[dict[str, object]] = []
    all_indices = list(range(len(proto_labels)))

    for row_index, predicted_label in enumerate(predicted_labels):
        predicted_indices = class_indices.get(predicted_label, [])
        competing_indices = [
            index for index in all_indices if proto_labels[index] != predicted_label
        ]

        predicted_similarity = 0.0
        predicted_family = None
        if predicted_indices:
            sims = matrix[row_index].dot(proto_matrix[predicted_indices].T).toarray().ravel()
            if sims.size:
                local = int(sims.argmax())
                proto_index = predicted_indices[local]
                predicted_similarity = float(sims[local])
                predicted_family = proto_families[proto_index]

        competing_similarity = 0.0
        competing_family = None
        competing_label = None
        if competing_indices:
            sims = matrix[row_index].dot(proto_matrix[competing_indices].T).toarray().ravel()
            if sims.size:
                local = int(sims.argmax())
                proto_index = competing_indices[local]
                competing_similarity = float(sims[local])
                competing_family = proto_families[proto_index]
                competing_label = proto_labels[proto_index]

        result.append(
            {
                "predicted_similarity": predicted_similarity,
                "predicted_family": predicted_family,
                "competing_similarity": competing_similarity,
                "competing_family": competing_family,
                "competing_label": competing_label,
                "contrast_margin": predicted_similarity - competing_similarity,
            }
        )

    return result


def nearest_family_by_predicted_class(
    vectorizer,
    prototypes,
    texts: Iterable[str],
    predicted_labels: Iterable[str],
) -> list[tuple[float, str | None]]:
    """Return max cosine similarity and nearest family for each predicted class."""
    scores = contrastive_family_scores(vectorizer, prototypes, texts, predicted_labels)
    return [
        (float(item["predicted_similarity"]), item["predicted_family"])
        for item in scores
    ]


def max_similarity_by_predicted_class(
    vectorizer,
    prototypes,
    texts: Iterable[str],
    predicted_labels: Iterable[str],
) -> list[float]:
    """Maximum cosine similarity to a family prototype of the predicted class."""
    return [
        similarity
        for similarity, _family in nearest_family_by_predicted_class(
            vectorizer,
            prototypes,
            texts,
            predicted_labels,
        )
    ]
