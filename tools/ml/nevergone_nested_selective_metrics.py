#!/usr/bin/env python3
"""Helpers for class-conditional selective routing summaries.

This module is intentionally model-agnostic. It summarizes held-out nested-CV
predictions by predicted class and action, including per-fold accept stability.
"""
from __future__ import annotations

from collections import defaultdict
from typing import Any, Iterable


def summarize_class_actions(
    records: Iterable[dict[str, Any]],
    classes: list[str],
    actions: tuple[str, ...] = ("accept", "review", "escalate"),
) -> dict[str, dict[str, dict[str, float | int]]]:
    totals: dict[str, dict[str, int]] = {
        label: {action: 0 for action in actions} for label in classes
    }
    correct: dict[str, dict[str, int]] = {
        label: {action: 0 for action in actions} for label in classes
    }

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
                "precision": (correct[label][action] / examples) if examples else 0.0,
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
        fold = int(record["outer_fold"])
        per_class_fold[label][fold].append(bool(record["correct"]))

    result: dict[str, dict[str, float | int | None]] = {}
    for label in classes:
        precisions = []
        examples = 0
        for fold_rows in per_class_fold[label].values():
            if not fold_rows:
                continue
            examples += len(fold_rows)
            precisions.append(sum(fold_rows) / len(fold_rows))

        result[label] = {
            "accept_examples": examples,
            "folds_with_accept": len(precisions),
            "total_folds": total_folds,
            "min_fold_precision": min(precisions) if precisions else None,
            "mean_fold_precision": (sum(precisions) / len(precisions)) if precisions else None,
        }
    return result
