#!/usr/bin/env python3
"""Evaluate label transfer from nearest known function families.

Every query function is scored only against families held out from its own
family. The TF-IDF vocabulary and family centroids are fit on each fold's
training families only. Function names, addresses, binary names and project
goals are never similarity inputs.

This tool is an evaluator, not a router. Threshold tables are descriptive and
must not be interpreted as independently tuned production policies.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from build_nevergone_similarity_index import load_dataset, load_family_map, require_dependencies

FORMAT = "NevergoneSimilarityTransferCV/1"


def _load_rows(
    dataset_dir: Path,
    families_path: Path,
    *,
    task: str,
    classes: list[str] | None,
) -> list[dict[str, Any]]:
    family_map = load_family_map(families_path)
    allowed = set(classes) if classes else None
    rows: list[dict[str, Any]] = []
    for row in load_dataset(dataset_dir):
        label = row["labels"].get(task)
        if not isinstance(label, str):
            continue
        if allowed is not None and label not in allowed:
            continue
        family_id = family_map.get(row["id"])
        if family_id is None:
            raise ValueError(f"dataset id missing from family map: {row['id']}")
        rows.append({
            "id": row["id"],
            "text": row["text"],
            "label": label,
            "family_id": family_id,
        })
    if not rows:
        raise ValueError("no labelled rows selected")
    return rows


def _prototype_index(deps, vectorizer, train_rows: list[dict[str, Any]]):
    matrix = vectorizer.transform([row["text"] for row in train_rows])
    by_family: dict[str, list[int]] = defaultdict(list)
    labels_by_family: dict[str, Counter[str]] = defaultdict(Counter)
    for index, row in enumerate(train_rows):
        family_id = row["family_id"]
        by_family[family_id].append(index)
        labels_by_family[family_id][row["label"]] += 1

    family_ids: list[str] = []
    family_labels: list[str] = []
    centroid_rows = []
    omitted_mixed = 0
    for family_id in sorted(by_family):
        label_counts = labels_by_family[family_id]
        if len(label_counts) != 1:
            omitted_mixed += 1
            continue
        indices = by_family[family_id]
        centroid = deps["sparse"].csr_matrix(matrix[indices].mean(axis=0))
        centroid = deps["normalize"](centroid, norm="l2", axis=1, copy=False)
        centroid_rows.append(centroid)
        family_ids.append(family_id)
        family_labels.append(next(iter(label_counts)))

    if not centroid_rows:
        raise ValueError("no unanimous training families available")
    prototypes = deps["sparse"].vstack(centroid_rows, format="csr")
    return prototypes, family_ids, family_labels, omitted_mixed


def _summary(correct: list[bool], selected: list[bool]) -> dict[str, float | int]:
    examples = sum(selected)
    good = sum(int(c) for c, keep in zip(correct, selected) if keep)
    return {
        "examples": examples,
        "coverage": examples / len(selected) if selected else 0.0,
        "precision": good / examples if examples else 0.0,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("families", type=Path)
    parser.add_argument("--task", required=True)
    parser.add_argument("--classes", nargs="*")
    parser.add_argument("--folds", type=int, default=5)
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()
    if args.folds < 2:
        raise SystemExit("--folds must be >= 2")

    deps = require_dependencies()
    try:
        import numpy as np
        from sklearn.model_selection import StratifiedGroupKFold
    except ImportError as exc:
        raise SystemExit("requires numpy and scikit-learn") from exc

    rows = _load_rows(
        args.dataset_dir,
        args.families,
        task=args.task,
        classes=args.classes or None,
    )
    labels = np.asarray([row["label"] for row in rows], dtype=object)
    groups = np.asarray([row["family_id"] for row in rows], dtype=object)
    texts = np.asarray([row["text"] for row in rows], dtype=object)

    print("model:", FORMAT)
    print("task:", args.task)
    print("functions:", len(rows))
    print("families:", len(set(groups.tolist())))
    print("classes:", Counter(labels.tolist()))
    print()

    splitter = StratifiedGroupKFold(
        n_splits=args.folds,
        shuffle=True,
        random_state=args.seed,
    )

    predictions: list[str | None] = [None] * len(rows)
    similarities = np.zeros(len(rows), dtype=float)
    gaps = np.zeros(len(rows), dtype=float)
    nearest_families: list[str | None] = [None] * len(rows)
    omitted_mixed_total = 0

    for fold, (train_idx, test_idx) in enumerate(splitter.split(texts, labels, groups), 1):
        vectorizer = deps["TfidfVectorizer"](
            tokenizer=str.split,
            token_pattern=None,
            lowercase=False,
            ngram_range=(1, 2),
            min_df=2,
            max_features=80000,
            sublinear_tf=True,
        )
        train_rows = [rows[i] for i in train_idx]
        test_rows = [rows[i] for i in test_idx]
        vectorizer.fit([row["text"] for row in train_rows])
        prototypes, family_ids, family_labels, omitted_mixed = _prototype_index(
            deps, vectorizer, train_rows
        )
        omitted_mixed_total += omitted_mixed

        query = vectorizer.transform([row["text"] for row in test_rows])
        query = deps["normalize"](query, norm="l2", axis=1, copy=False)
        scores = query @ prototypes.T

        fold_correct = 0
        for local_index, global_index in enumerate(test_idx):
            dense = scores.getrow(local_index).toarray().ravel()
            order = np.argsort(-dense, kind="stable")
            first = int(order[0])
            second_score = float(dense[int(order[1])]) if len(order) > 1 else 0.0
            top_score = float(dense[first])
            pred = family_labels[first]
            predictions[global_index] = pred
            similarities[global_index] = top_score
            gaps[global_index] = top_score - second_score
            nearest_families[global_index] = family_ids[first]
            fold_correct += int(pred == labels[global_index])

        print(
            f"fold={fold} n={len(test_idx)} families={len(set(groups[test_idx].tolist()))} "
            f"accuracy={fold_correct / len(test_idx):.4f} prototypes={len(family_ids)} "
            f"mixed_train_families_omitted={omitted_mixed}"
        )

    pred = [str(value) for value in predictions]
    correct = [p == str(y) for p, y in zip(pred, labels.tolist())]
    accuracy = sum(correct) / len(correct)

    print()
    print("family-held-out transfer accuracy:", f"{accuracy:.4f}")
    print("mixed training families omitted across folds:", omitted_mixed_total)

    print("\nCONFUSION")
    classes = sorted(set(labels.tolist()) | set(pred))
    print("order:", classes)
    for truth in classes:
        counts = Counter(
            p for p, y in zip(pred, labels.tolist()) if y == truth
        )
        print(" ", truth, {label: counts[label] for label in classes})

    print("\nPRECISION/COVERAGE GRID")
    sim_thresholds = (0.95, 0.90, 0.85, 0.80, 0.70, 0.60)
    gap_thresholds = (0.30, 0.20, 0.10, 0.05, 0.00)
    for sim in sim_thresholds:
        for gap in gap_thresholds:
            selected = [
                float(s) >= sim and float(g) >= gap
                for s, g in zip(similarities.tolist(), gaps.tolist())
            ]
            report = _summary(correct, selected)
            print(
                f"sim>={sim:.2f} gap>={gap:.2f} "
                f"examples={report['examples']} coverage={report['coverage']:.4f} "
                f"precision={report['precision']:.4f}"
            )

    print("\nPER PREDICTED CLASS")
    for label in classes:
        mask = [p == label for p in pred]
        examples = sum(mask)
        good = sum(int(c) for c, keep in zip(correct, mask) if keep)
        avg_sim = (
            sum(float(s) for s, keep in zip(similarities.tolist(), mask) if keep) / examples
            if examples else 0.0
        )
        avg_gap = (
            sum(float(g) for g, keep in zip(gaps.tolist(), mask) if keep) / examples
            if examples else 0.0
        )
        print(
            f"  {label}: examples={examples} precision={good/examples if examples else 0.0:.4f} "
            f"sim_avg={avg_sim:.4f} gap_avg={avg_gap:.4f}"
        )

    print("\nTOP HIGH-CONFIDENCE ERRORS")
    errors = [
        (float(similarities[i]), float(gaps[i]), rows[i]["id"], str(labels[i]), pred[i], nearest_families[i])
        for i in range(len(rows)) if not correct[i]
    ]
    for sim, gap, record_id, truth, predicted, family_id in sorted(errors, reverse=True)[:20]:
        print(
            f"  {record_id} truth={truth} pred={predicted} sim={sim:.4f} gap={gap:.4f} family={family_id}"
        )

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
