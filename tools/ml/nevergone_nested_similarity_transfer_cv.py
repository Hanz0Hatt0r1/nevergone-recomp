#!/usr/bin/env python3
"""Nested family-held-out validation for nearest-family label transfer.

The outer fold is never used to choose similarity/gap policies. For each outer
training partition, inner family-grouped OOF retrieval chooses a per-predicted-
class similarity/gap policy. The policy is then evaluated on the untouched
outer families.

Names, addresses, binary names, strings and project goals are not retrieval
inputs. This is still an evaluator: it does not change production routing.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from build_nevergone_similarity_index import require_dependencies
from nevergone_similarity_transfer_cv import _load_rows, _prototype_index

FORMAT = "NevergoneNestedSimilarityTransferCV/1"
SIM_THRESHOLDS = (0.98, 0.95, 0.92, 0.90, 0.88, 0.85, 0.82, 0.80, 0.75, 0.70, 0.60)
GAP_THRESHOLDS = (0.30, 0.20, 0.15, 0.10, 0.075, 0.05, 0.025, 0.00)


def select_transfer_policy(
    records: list[dict[str, Any]],
    *,
    target_precision: float = 0.95,
    min_examples: int = 10,
) -> dict[str, dict[str, Any]]:
    """Select highest-coverage threshold pair independently per predicted class."""
    policies: dict[str, dict[str, Any]] = {}
    for predicted in sorted({str(row["prediction"]) for row in records}):
        class_rows = [row for row in records if str(row["prediction"]) == predicted]
        best: tuple[int, float, float, float] | None = None
        for sim_threshold in SIM_THRESHOLDS:
            for gap_threshold in GAP_THRESHOLDS:
                selected = [
                    row for row in class_rows
                    if float(row["similarity"]) >= sim_threshold
                    and float(row["gap"]) >= gap_threshold
                ]
                if len(selected) < min_examples:
                    continue
                precision = sum(bool(row["correct"]) for row in selected) / len(selected)
                if precision + 1e-12 < target_precision:
                    continue
                candidate = (len(selected), precision, sim_threshold, gap_threshold)
                if best is None or candidate > best:
                    best = candidate
        if best is None:
            policies[predicted] = {
                "enabled": False,
                "examples": 0,
                "precision": None,
                "similarity_threshold": None,
                "gap_threshold": None,
            }
        else:
            examples, precision, sim_threshold, gap_threshold = best
            policies[predicted] = {
                "enabled": True,
                "examples": examples,
                "precision": precision,
                "similarity_threshold": sim_threshold,
                "gap_threshold": gap_threshold,
            }
    return policies


def policy_accepts(row: dict[str, Any], policies: dict[str, dict[str, Any]]) -> bool:
    policy = policies.get(str(row["prediction"]))
    if not policy or not policy.get("enabled"):
        return False
    return (
        float(row["similarity"]) >= float(policy["similarity_threshold"])
        and float(row["gap"]) >= float(policy["gap_threshold"])
    )


def _fit_reference(deps, train_rows: list[dict[str, Any]]):
    vectorizer = deps["TfidfVectorizer"](
        tokenizer=str.split,
        token_pattern=None,
        lowercase=False,
        ngram_range=(1, 2),
        min_df=2,
        max_features=80000,
        sublinear_tf=True,
    )
    vectorizer.fit([row["text"] for row in train_rows])
    prototypes, family_ids, family_labels, omitted_mixed = _prototype_index(
        deps, vectorizer, train_rows
    )
    return vectorizer, prototypes, family_ids, family_labels, omitted_mixed


def _score_rows(deps, np, reference, test_rows: list[dict[str, Any]]) -> list[dict[str, Any]]:
    vectorizer, prototypes, family_ids, family_labels, _ = reference
    query = vectorizer.transform([row["text"] for row in test_rows])
    query = deps["normalize"](query, norm="l2", axis=1, copy=False)
    scores = query @ prototypes.T
    output: list[dict[str, Any]] = []
    for index, row in enumerate(test_rows):
        dense = scores.getrow(index).toarray().ravel()
        order = np.argsort(-dense, kind="stable")
        first = int(order[0])
        top_score = float(dense[first])
        second_score = float(dense[int(order[1])]) if len(order) > 1 else 0.0
        prediction = str(family_labels[first])
        output.append({
            "id": row["id"],
            "truth": row["label"],
            "prediction": prediction,
            "correct": prediction == row["label"],
            "similarity": top_score,
            "gap": top_score - second_score,
            "nearest_family": family_ids[first],
        })
    return output


def _inner_oof(deps, np, StratifiedGroupKFold, rows: list[dict[str, Any]], folds: int, seed: int):
    labels = np.asarray([row["label"] for row in rows], dtype=object)
    groups = np.asarray([row["family_id"] for row in rows], dtype=object)
    dummy = np.arange(len(rows))
    splitter = StratifiedGroupKFold(n_splits=folds, shuffle=True, random_state=seed)
    records: list[dict[str, Any]] = []
    for train_idx, test_idx in splitter.split(dummy, labels, groups):
        train_rows = [rows[int(i)] for i in train_idx]
        test_rows = [rows[int(i)] for i in test_idx]
        reference = _fit_reference(deps, train_rows)
        records.extend(_score_rows(deps, np, reference, test_rows))
    return records


def _selective_summary(rows: list[dict[str, Any]]) -> dict[str, Any]:
    accepted = [row for row in rows if row["accepted"]]
    correct = sum(bool(row["correct"]) for row in accepted)
    return {
        "examples": len(accepted),
        "coverage": len(accepted) / len(rows) if rows else 0.0,
        "precision": correct / len(accepted) if accepted else 0.0,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dataset_dir", type=Path)
    parser.add_argument("families", type=Path)
    parser.add_argument("--task", required=True)
    parser.add_argument("--classes", nargs="*")
    parser.add_argument("--outer-folds", type=int, default=5)
    parser.add_argument("--inner-folds", type=int, default=4)
    parser.add_argument("--target-precision", type=float, default=0.95)
    parser.add_argument("--min-policy-examples", type=int, default=10)
    parser.add_argument("--seed", type=int, default=42)
    args = parser.parse_args()
    if args.outer_folds < 2 or args.inner_folds < 2:
        raise SystemExit("outer/inner folds must be >= 2")
    if not (0.0 < args.target_precision <= 1.0):
        raise SystemExit("--target-precision must be in (0,1]")
    if args.min_policy_examples < 1:
        raise SystemExit("--min-policy-examples must be >= 1")

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
    dummy = np.arange(len(rows))

    print("model:", FORMAT)
    print("task:", args.task)
    print("functions:", len(rows))
    print("families:", len(set(groups.tolist())))
    print("classes:", Counter(labels.tolist()))
    print("target precision:", args.target_precision)
    print("min policy examples:", args.min_policy_examples)
    print()

    outer = StratifiedGroupKFold(
        n_splits=args.outer_folds,
        shuffle=True,
        random_state=args.seed,
    )
    all_outer: list[dict[str, Any]] = []
    policy_history: dict[str, list[dict[str, Any]]] = defaultdict(list)

    for fold, (outer_train_idx, outer_test_idx) in enumerate(outer.split(dummy, labels, groups), 1):
        outer_train = [rows[int(i)] for i in outer_train_idx]
        outer_test = [rows[int(i)] for i in outer_test_idx]

        inner_records = _inner_oof(
            deps,
            np,
            StratifiedGroupKFold,
            outer_train,
            args.inner_folds,
            args.seed + fold * 1009,
        )
        policies = select_transfer_policy(
            inner_records,
            target_precision=args.target_precision,
            min_examples=args.min_policy_examples,
        )
        for label, policy in policies.items():
            policy_history[label].append(policy)

        reference = _fit_reference(deps, outer_train)
        outer_records = _score_rows(deps, np, reference, outer_test)
        for row in outer_records:
            row["outer_fold"] = fold
            row["accepted"] = policy_accepts(row, policies)
        all_outer.extend(outer_records)

        raw_accuracy = sum(bool(row["correct"]) for row in outer_records) / len(outer_records)
        selective = _selective_summary(outer_records)
        enabled = {
            label: {
                "sim": policy["similarity_threshold"],
                "gap": policy["gap_threshold"],
                "inner_n": policy["examples"],
                "inner_precision": policy["precision"],
            }
            for label, policy in policies.items() if policy.get("enabled")
        }
        print(
            f"outer_fold={fold} n={len(outer_records)} raw_accuracy={raw_accuracy:.4f} "
            f"accept={selective['examples']} coverage={selective['coverage']:.4f} "
            f"precision={selective['precision']:.4f} enabled_policies={enabled}"
        )

    raw_accuracy = sum(bool(row["correct"]) for row in all_outer) / len(all_outer)
    selective = _selective_summary(all_outer)
    print("\nnested raw transfer accuracy:", f"{raw_accuracy:.4f}")
    print("nested selective:", selective)

    print("\nNESTED ACCEPT BY PREDICTED CLASS")
    for label in sorted({str(row["prediction"]) for row in all_outer}):
        selected = [row for row in all_outer if row["prediction"] == label and row["accepted"]]
        correct = sum(bool(row["correct"]) for row in selected)
        print(
            f"  {label}: examples={len(selected)} correct={correct} "
            f"precision={correct/len(selected) if selected else 0.0:.4f}"
        )

    print("\nPOLICY STABILITY")
    for label in sorted(policy_history):
        history = policy_history[label]
        enabled = [policy for policy in history if policy.get("enabled")]
        sims = sorted({policy["similarity_threshold"] for policy in enabled})
        gaps = sorted({policy["gap_threshold"] for policy in enabled})
        inner_precisions = [float(policy["precision"]) for policy in enabled]
        print(
            f"  {label}: enabled_folds={len(enabled)}/{len(history)} "
            f"sim_thresholds={sims} gap_thresholds={gaps} "
            f"inner_precision_min={min(inner_precisions) if inner_precisions else None}"
        )

    print("\nACCEPTED ERRORS")
    errors = [row for row in all_outer if row["accepted"] and not row["correct"]]
    for row in sorted(errors, key=lambda item: (-float(item["similarity"]), -float(item["gap"])))[:30]:
        print(
            f"  {row['id']} truth={row['truth']} pred={row['prediction']} "
            f"sim={row['similarity']:.4f} gap={row['gap']:.4f} "
            f"family={row['nearest_family']} fold={row['outer_fold']}"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
