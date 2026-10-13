#!/usr/bin/env python3
"""Evaluate Nevergone code-domain classification with near-duplicate family groups."""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path

import numpy as np
from sklearn.feature_extraction.text import TfidfVectorizer
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import (
    balanced_accuracy_score,
    classification_report,
    confusion_matrix,
    f1_score,
)
from sklearn.model_selection import StratifiedGroupKFold
from sklearn.pipeline import Pipeline

from nevergone_pcode_features import portable_pcode


def read_rows(dataset_dir: Path) -> list[dict]:
    rows: list[dict] = []
    seen: set[str] = set()
    for split in ("train", "validation", "test"):
        for raw in (dataset_dir / f"{split}.jsonl").read_text(encoding="utf-8").splitlines():
            if not raw.strip():
                continue
            row = json.loads(raw)
            record_id = row["id"]
            if record_id in seen:
                raise ValueError(f"duplicate dataset id: {record_id}")
            seen.add(record_id)
            rows.append(row)
    return rows


def read_families(path: Path) -> dict[str, str]:
    result: dict[str, str] = {}
    for line_no, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not raw.strip():
            continue
        row = json.loads(raw)
        record_id = row.get("id")
        family_id = row.get("family_id")
        if not isinstance(record_id, str) or not isinstance(family_id, str):
            raise ValueError(f"{path}:{line_no}: invalid family row")
        if record_id in result:
            raise ValueError(f"duplicate family id mapping: {record_id}")
        result[record_id] = family_id
    return result


def make_model() -> Pipeline:
    return Pipeline([
        (
            "tfidf",
            TfidfVectorizer(
                tokenizer=str.split,
                token_pattern=None,
                lowercase=False,
                ngram_range=(1, 2),
                min_df=2,
                max_features=80000,
                sublinear_tf=True,
            ),
        ),
        (
            "classifier",
            LogisticRegression(
                solver="lbfgs",
                max_iter=4000,
                class_weight="balanced",
                random_state=42,
            ),
        ),
    ])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("families", type=Path)
    parser.add_argument("--task", default="code_domain")
    parser.add_argument("--folds", type=int, default=5)
    parser.add_argument("--classes", nargs="*")
    args = parser.parse_args()

    family_by_id = read_families(args.families)
    selected: list[tuple[str, str, str]] = []
    for row in read_rows(args.dataset_dir):
        labels = row.get("labels", {})
        label = labels.get(args.task) if isinstance(labels, dict) else None
        if not isinstance(label, str):
            continue
        if args.classes and label not in set(args.classes):
            continue
        record_id = row["id"]
        if record_id not in family_by_id:
            raise ValueError(f"missing family assignment for dataset id {record_id}")
        selected.append((portable_pcode(row["text"]), label, family_by_id[record_id]))

    if not selected:
        raise ValueError("no labelled rows selected")

    labels_order = args.classes or sorted({label for _, label, _ in selected})
    X = np.asarray([text for text, _, _ in selected], dtype=object)
    y = np.asarray([label for _, label, _ in selected], dtype=object)
    groups = np.asarray([family for _, _, family in selected], dtype=object)

    print("functions:", len(selected))
    print("families:", len(set(groups)))
    print("classes:", Counter(y))
    print()

    splitter = StratifiedGroupKFold(
        n_splits=args.folds,
        shuffle=True,
        random_state=42,
    )
    balanced_scores: list[float] = []
    macro_scores: list[float] = []
    all_true: list[str] = []
    all_pred: list[str] = []

    for fold, (train_idx, test_idx) in enumerate(splitter.split(X, y, groups), 1):
        model = make_model()
        model.fit(X[train_idx].tolist(), y[train_idx].tolist())
        pred = model.predict(X[test_idx].tolist())
        true = y[test_idx]
        balanced = balanced_accuracy_score(true, pred)
        macro = f1_score(true, pred, average="macro")
        balanced_scores.append(balanced)
        macro_scores.append(macro)
        all_true.extend(true.tolist())
        all_pred.extend(pred.tolist())
        print(
            f"fold={fold} balanced={balanced:.4f} "
            f"macro_f1={macro:.4f} n={len(test_idx)} families={len(set(groups[test_idx]))}"
        )

    print()
    print(f"balanced: {np.mean(balanced_scores):.4f} +/- {np.std(balanced_scores):.4f}")
    print(f"macro_f1: {np.mean(macro_scores):.4f} +/- {np.std(macro_scores):.4f}")
    print()
    print("CONFUSION MATRIX")
    print("order:", labels_order)
    print(confusion_matrix(all_true, all_pred, labels=labels_order))
    print()
    print("CLASSIFICATION REPORT")
    print(
        classification_report(
            all_true,
            all_pred,
            labels=labels_order,
            digits=4,
            zero_division=0,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
