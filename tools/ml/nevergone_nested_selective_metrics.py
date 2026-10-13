#!/usr/bin/env python3
"""Helpers for class-conditional selective routing summaries."""
from __future__ import annotations

from collections import defaultdict
from typing import Any, Iterable


def summarize_class_actions(
    records: Iterable[dict[str, Any]],
    classes: list[str],
    actions: tuple[str, ...] = ("accept", "review", "escalate"),
) -> dict[str, dict[str, dict[str, float | int]]]:
    totals = {label: {action: 0 for action in actions} for label in classes}
    correct = {label: {action: 0 for action in actions} for label in classes}
    for record in records:
        label = str(record["prediction"])
        action = str(record["action"])
        if label not in totals or action not in totals[label]:
            continue
        totals[label][action] += 1
        correct[label][action] += int(bool(record["correct"]))

    summary: dict[str, dict[str, dict[str, float | int]]] = {}
    for label in classes:
        summary[label] = {}
        for action in actions:
            examples = totals[label][action]
            summary[label][action] = {
                "examples": examples,
                "correct": correct[label][action],
                "precision": correct[label][action] / examples if examples else 0.0,
            }
    return summary


def summarize_accept_stability(
    records: Iterable[dict[str, Any]],
    classes: list[str],
    total_folds: int,
) -> dict[str, dict[str, float | int | None]]:
    per_class_fold: dict[str, dict[int, list[bool]]] = {
        label: defaultdict(list) for label in classes
    }
    for record in records:
        if record.get("action") != "accept":
            continue
        label = str(record["prediction"])
        if label not in per_class_fold:
            continue
        per_class_fold[label][int(record["outer_fold"])].append(bool(record["correct"]))

    result: dict[str, dict[str, float | int | None]] = {}
    for label in classes:
        precisions: list[float] = []
        examples = 0
        for rows in per_class_fold[label].values():
            if not rows:
                continue
            examples += len(rows)
            precisions.append(sum(rows) / len(rows))
        result[label] = {
            "accept_examples": examples,
            "folds_with_accept": len(precisions),
            "total_folds": total_folds,
            "min_fold_precision": min(precisions) if precisions else None,
            "mean_fold_precision": sum(precisions) / len(precisions) if precisions else None,
        }
    return result
