#!/usr/bin/env python3
"""Evaluate v6 production routing with the v5.6 GAME trust meta score.

This experiment deliberately does not relabel functions.  The v5.2 class
prediction is frozen.  Only routing is allowed to become more conservative:
GAME rows that were ACCEPT/REVIEW may be downgraded according to the leakage-safe
v5.6 meta keep probability.

For each held-out outer fold, routing thresholds are selected from all *other*
outer OOF folds.  Thus the target fold labels never participate in threshold
selection.  The meta probabilities themselves are already outer-family-held-out
predictions produced by v5.6.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.GameMetaRoutingV6/1"
ACTIONS = ("accept", "review", "escalate")
CLASSES = ("game", "engine", "runtime_library", "media_library")


def read_jsonl(path: Path) -> dict[str, dict[str, Any]]:
    rows: dict[str, dict[str, Any]] = {}
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            record_id = row.get("id")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError(f"{path}:{line_no}: missing id")
            if record_id in rows:
                raise ValueError(f"{path}:{line_no}: duplicate id {record_id}")
            rows[record_id] = row
    return rows


def class_metrics(rows: list[dict[str, Any]]) -> dict[str, float]:
    truth = [str(row["truth"]) for row in rows]
    pred = [str(row["prediction"]) for row in rows]
    accuracy = sum(t == p for t, p in zip(truth, pred)) / max(1, len(rows))
    recalls: list[float] = []
    f1s: list[float] = []
    for label in CLASSES:
        tp = sum(t == label and p == label for t, p in zip(truth, pred))
        fp = sum(t != label and p == label for t, p in zip(truth, pred))
        fn = sum(t == label and p != label for t, p in zip(truth, pred))
        recall = tp / max(1, tp + fn)
        precision = tp / max(1, tp + fp)
        f1 = 2.0 * precision * recall / max(1e-12, precision + recall)
        recalls.append(recall)
        f1s.append(f1)
    return {
        "accuracy": accuracy,
        "balanced_accuracy": sum(recalls) / len(recalls),
        "macro_f1": sum(f1s) / len(f1s),
    }


def action_summary(rows: list[dict[str, Any]], action_key: str) -> dict[str, dict[str, float]]:
    result: dict[str, dict[str, float]] = {}
    total = max(1, len(rows))
    for action in ACTIONS:
        selected = [row for row in rows if str(row[action_key]) == action]
        correct = sum(bool(row["correct"]) for row in selected)
        result[action] = {
            "examples": len(selected),
            "coverage": len(selected) / total,
            "precision": correct / max(1, len(selected)),
        }
    return result


def threshold_grid() -> list[float]:
    return [round(value / 20.0, 2) for value in range(0, 20)] + [0.99]


def routed_action(row: dict[str, Any], accept_threshold: float, review_threshold: float) -> str:
    base_action = str(row["base_action"])
    if str(row["prediction"]) != "game" or base_action == "escalate":
        return base_action
    score = float(row["meta_keep_game_probability"])
    if base_action == "accept":
        if score >= accept_threshold:
            return "accept"
        if score >= review_threshold:
            return "review"
        return "escalate"
    if base_action == "review":
        return "review" if score >= review_threshold else "escalate"
    return base_action


def apply_thresholds(
    rows: list[dict[str, Any]], accept_threshold: float, review_threshold: float
) -> list[str]:
    return [routed_action(row, accept_threshold, review_threshold) for row in rows]


def summarize_actions(rows: list[dict[str, Any]], actions: list[str]) -> dict[str, dict[str, float]]:
    total = max(1, len(rows))
    result: dict[str, dict[str, float]] = {}
    for action in ACTIONS:
        indices = [index for index, value in enumerate(actions) if value == action]
        correct = sum(bool(rows[index]["correct"]) for index in indices)
        result[action] = {
            "examples": len(indices),
            "coverage": len(indices) / total,
            "precision": correct / max(1, len(indices)),
        }
    return result


def select_thresholds(
    rows: list[dict[str, Any]],
    *,
    target_accept_precision: float,
    target_review_precision: float,
    max_accept_coverage_loss: float,
) -> tuple[dict[str, float], list[dict[str, Any]]]:
    base = action_summary(rows, "base_action")
    base_accept_coverage = base["accept"]["coverage"]
    reports: list[dict[str, Any]] = []

    # accept threshold must be at least as strict as review threshold.
    for review_threshold in threshold_grid():
        for accept_threshold in threshold_grid():
            if accept_threshold < review_threshold:
                continue
            actions = apply_thresholds(rows, accept_threshold, review_threshold)
            summary = summarize_actions(rows, actions)
            accept = summary["accept"]
            review = summary["review"]
            eligible = bool(
                accept["examples"] > 0
                and accept["precision"] >= target_accept_precision
                and accept["coverage"] >= base_accept_coverage - max_accept_coverage_loss
                and (review["examples"] == 0 or review["precision"] >= target_review_precision)
            )
            reports.append(
                {
                    "accept_threshold": accept_threshold,
                    "review_threshold": review_threshold,
                    "eligible": eligible,
                    "actions": summary,
                }
            )

    eligible = [row for row in reports if row["eligible"]]
    if not eligible:
        return {"accept_threshold": 0.0, "review_threshold": 0.0, "fallback": True}, reports

    selected = max(
        eligible,
        key=lambda row: (
            row["actions"]["accept"]["coverage"],
            row["actions"]["accept"]["precision"],
            row["actions"]["review"]["precision"],
            -row["actions"]["escalate"]["coverage"],
            -row["accept_threshold"],
            -row["review_threshold"],
        ),
    )
    return {
        "accept_threshold": float(selected["accept_threshold"]),
        "review_threshold": float(selected["review_threshold"]),
        "fallback": False,
    }, reports


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("base_oof", type=Path, help="v5.2 oof_predictions.jsonl")
    parser.add_argument("meta_oof", type=Path, help="v5.6 oof_meta_verifier_predictions.jsonl")
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--target-accept-precision", type=float, default=0.97)
    parser.add_argument("--target-review-precision", type=float, default=0.80)
    parser.add_argument("--max-accept-coverage-loss", type=float, default=0.04)
    args = parser.parse_args()

    for value, name in (
        (args.target_accept_precision, "--target-accept-precision"),
        (args.target_review_precision, "--target-review-precision"),
        (args.max_accept_coverage_loss, "--max-accept-coverage-loss"),
    ):
        if not 0.0 <= value <= 1.0:
            raise SystemExit(f"{name} must be in [0, 1]")

    base = read_jsonl(args.base_oof)
    meta = read_jsonl(args.meta_oof)
    if set(base) != set(meta):
        missing_meta = sorted(set(base) - set(meta))
        missing_base = sorted(set(meta) - set(base))
        raise ValueError(
            f"OOF id mismatch: missing_meta={len(missing_meta)} missing_base={len(missing_base)}"
        )

    rows: list[dict[str, Any]] = []
    for record_id in base:
        base_row = base[record_id]
        meta_row = meta[record_id]
        prediction = str(base_row.get("prediction"))
        if prediction != str(meta_row.get("base_prediction")):
            raise ValueError(f"prediction mismatch for {record_id}")
        truth = str(base_row.get("truth"))
        meta_label = str(meta_row.get("label"))
        if truth != meta_label:
            raise ValueError(f"truth mismatch for {record_id}")
        fold = int(meta_row.get("fold", 0))
        if fold <= 0:
            raise ValueError(f"missing fold for {record_id}")
        rows.append(
            {
                "id": record_id,
                "family_id": base_row.get("family_id"),
                "fold": fold,
                "truth": truth,
                "prediction": prediction,
                "correct": bool(base_row.get("correct", prediction == truth)),
                "base_action": str(base_row.get("action")),
                "meta_keep_game_probability": float(meta_row.get("meta_keep_game_probability", 1.0)),
            }
        )

    folds = sorted({int(row["fold"]) for row in rows})
    if len(folds) < 2:
        raise ValueError("need at least two outer folds for cross-fitted routing calibration")

    routed_by_id: dict[str, str] = {}
    fold_reports: list[dict[str, Any]] = []
    selected_thresholds: list[dict[str, Any]] = []
    for fold in folds:
        train = [row for row in rows if int(row["fold"]) != fold]
        test = [row for row in rows if int(row["fold"]) == fold]
        selected, candidates = select_thresholds(
            train,
            target_accept_precision=args.target_accept_precision,
            target_review_precision=args.target_review_precision,
            max_accept_coverage_loss=args.max_accept_coverage_loss,
        )
        test_actions = apply_thresholds(
            test,
            float(selected["accept_threshold"]),
            float(selected["review_threshold"]),
        )
        for row, action in zip(test, test_actions):
            routed_by_id[str(row["id"])] = action
        selected_thresholds.append({"fold": fold, **selected})
        fold_reports.append(
            {
                "fold": fold,
                "train_examples": len(train),
                "test_examples": len(test),
                "selected": selected,
                "eligible_candidates": sum(bool(row["eligible"]) for row in candidates),
                "base": action_summary(test, "base_action"),
                "routed": summarize_actions(test, test_actions),
            }
        )

    if len(routed_by_id) != len(rows):
        raise RuntimeError("not every OOF row received a routed action")

    routed_actions = [routed_by_id[str(row["id"])] for row in rows]
    base_summary = action_summary(rows, "base_action")
    routed_summary = summarize_actions(rows, routed_actions)
    changed = [
        index for index, row in enumerate(rows)
        if str(row["base_action"]) != routed_actions[index]
    ]
    changed_correct = sum(bool(rows[index]["correct"]) for index in changed)
    changed_wrong = len(changed) - changed_correct
    downgraded_accepts = sum(
        str(rows[index]["base_action"]) == "accept" and routed_actions[index] != "accept"
        for index in changed
    )
    game_accept_before = [
        row for row in rows if row["prediction"] == "game" and row["base_action"] == "accept"
    ]
    game_accept_after = [
        row for row, action in zip(rows, routed_actions)
        if row["prediction"] == "game" and action == "accept"
    ]

    output_dir = args.output_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    rows_path = output_dir / "oof_meta_routing_predictions.jsonl"
    summary_path = output_dir / "summary.json"
    with rows_path.open("w", encoding="utf-8") as handle:
        for row, action in zip(rows, routed_actions):
            handle.write(
                json.dumps(
                    {
                        "format": FORMAT,
                        **row,
                        "routed_action": action,
                        "changed": action != row["base_action"],
                    },
                    sort_keys=True,
                )
                + "\n"
            )

    summary = {
        "format": FORMAT,
        "training_feedback": False,
        "class_predictions_changed": False,
        "rows": len(rows),
        "folds": folds,
        "targets": {
            "accept_precision": args.target_accept_precision,
            "review_precision": args.target_review_precision,
            "max_accept_coverage_loss": args.max_accept_coverage_loss,
        },
        "class_metrics": class_metrics(rows),
        "base_actions": base_summary,
        "routed_actions": routed_summary,
        "selected_thresholds": selected_thresholds,
        "changed_actions": len(changed),
        "changed_correct_predictions": changed_correct,
        "changed_incorrect_predictions": changed_wrong,
        "downgraded_accepts": downgraded_accepts,
        "game_accept": {
            "before_examples": len(game_accept_before),
            "before_precision": sum(bool(row["correct"]) for row in game_accept_before) / max(1, len(game_accept_before)),
            "after_examples": len(game_accept_after),
            "after_precision": sum(bool(row["correct"]) for row in game_accept_after) / max(1, len(game_accept_after)),
        },
        "fold_reports": fold_reports,
        "rows_path": str(rows_path),
    }
    summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print(f"format: {FORMAT}")
    print("rows:", len(rows))
    print("class predictions changed: no")
    metrics = summary["class_metrics"]
    print(
        "class metrics:",
        f"balanced={metrics['balanced_accuracy']:.4f}",
        f"macro_f1={metrics['macro_f1']:.4f}",
    )
    print("selected thresholds:", selected_thresholds)
    print("base routing:", base_summary)
    print("meta routing:", routed_summary)
    print(
        "GAME accept:",
        f"before={len(game_accept_before)}@{summary['game_accept']['before_precision']:.4f}",
        f"after={len(game_accept_after)}@{summary['game_accept']['after_precision']:.4f}",
    )
    print(
        "changed actions:", len(changed),
        "changed-correct:", changed_correct,
        "changed-wrong:", changed_wrong,
        "downgraded accepts:", downgraded_accepts,
    )
    print("training feedback: no")
    print("rows:", rows_path)
    print("summary:", summary_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
