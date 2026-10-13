#!/usr/bin/env python3
"""Audit function_role predictions on a fresh identity-unseen corpus.

Ground truth for this audit is deliberately limited to conservative role labels
that can be inferred from recovered Ghidra names by the same provenance rules
used to build the training labels. Names are used only for audit/evaluation and
never enter model inference.

This is therefore a held-out label-rule agreement audit, not a claim of full
semantic ground truth for every function.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from augment_nevergone_function_roles import infer_function_role


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            rows.append(row)
    return rows


def stable_row_id(row: dict[str, Any], index: int) -> str:
    value = row.get("id")
    if isinstance(value, str) and value:
        return value
    program = row.get("program")
    entry = row.get("source_entry")
    if isinstance(program, str) and isinstance(entry, str):
        return f"{program}:{entry}"
    return f"row-{index + 1}"


def precision(correct: int, total: int) -> float:
    return (correct / total) if total else 0.0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path, help="fresh identity-unseen P-code corpus")
    parser.add_argument("predictions", type=Path, help="hierarchy-with-role JSONL")
    parser.add_argument(
        "--classes",
        nargs="+",
        default=["getter", "setter", "destructor", "callback"],
    )
    args = parser.parse_args()

    corpus = read_jsonl(args.corpus)
    predicted = read_jsonl(args.predictions)
    if len(corpus) != len(predicted):
        raise SystemExit(
            f"row count mismatch: corpus={len(corpus)} predictions={len(predicted)}"
        )

    corpus_by_id: dict[str, dict[str, Any]] = {}
    for index, row in enumerate(corpus):
        row_id = stable_row_id(row, index)
        if row_id in corpus_by_id:
            raise ValueError(f"duplicate corpus id: {row_id}")
        corpus_by_id[row_id] = row

    truth_counts: Counter[str] = Counter()
    pred_counts: Counter[str] = Counter()
    confusion: Counter[tuple[str, str]] = Counter()
    per_prediction: dict[str, Counter[str]] = defaultdict(Counter)
    per_action: dict[str, Counter[str]] = defaultdict(Counter)
    labelled = 0
    unlabelled = 0
    callback_rows: list[tuple[str, str | None, str, str]] = []

    for index, result in enumerate(predicted):
        row_id = stable_row_id(result, index)
        source = corpus_by_id.get(row_id)
        if source is None:
            raise ValueError(f"prediction id not found in corpus: {row_id}")

        role = result.get("function_role")
        if not isinstance(role, dict):
            continue
        prediction = role.get("prediction")
        action = role.get("action")
        if not isinstance(prediction, str) or not isinstance(action, str):
            continue
        pred_counts[prediction] += 1

        name = source.get("source_name")
        inferred = infer_function_role(name) if isinstance(name, str) else None
        truth = inferred[0] if inferred is not None else None

        if prediction == "callback":
            callback_rows.append((row_id, truth, action, name if isinstance(name, str) else ""))

        if truth is None or truth not in args.classes:
            unlabelled += 1
            continue

        labelled += 1
        truth_counts[truth] += 1
        confusion[(truth, prediction)] += 1
        stats = per_prediction[prediction]
        stats["examples"] += 1
        stats["correct"] += int(prediction == truth)
        action_stats = per_action[f"{prediction}:{action}"]
        action_stats["examples"] += 1
        action_stats["correct"] += int(prediction == truth)

    print("model: NevergoneFreshRoleAudit/1")
    print("functions:", len(corpus))
    print("role predictions:", pred_counts)
    print("audit-labelled:", labelled)
    print("audit-unlabelled:", unlabelled)
    print("truth distribution:", truth_counts)

    print("\nprecision by predicted class on audit-labelled rows:")
    for label in args.classes:
        stats = per_prediction[label]
        print(
            f"  {label}: examples={stats['examples']} correct={stats['correct']} "
            f"precision={precision(stats['correct'], stats['examples']):.4f}"
        )

    print("\nprecision by predicted class/action on audit-labelled rows:")
    for label in args.classes:
        for action in ("accept", "review", "escalate"):
            stats = per_action[f"{label}:{action}"]
            print(
                f"  {label}:{action}: examples={stats['examples']} "
                f"correct={stats['correct']} "
                f"precision={precision(stats['correct'], stats['examples']):.4f}"
            )

    print("\nconfusion (truth -> prediction):")
    for truth in args.classes:
        row = {pred: confusion[(truth, pred)] for pred in args.classes}
        print(f"  {truth}: {row}")

    callback_labelled = [row for row in callback_rows if row[1] is not None]
    callback_correct = sum(1 for _, truth, _, _ in callback_labelled if truth == "callback")
    print("\ncallback external audit:")
    print("  predicted callbacks:", len(callback_rows))
    print("  audit-labelled callbacks:", len(callback_labelled))
    print("  correct labelled callbacks:", callback_correct)
    print(
        "  labelled callback precision:",
        f"{precision(callback_correct, len(callback_labelled)):.4f}",
    )
    print("  actions:", Counter(action for _, _, action, _ in callback_rows))

    print("\ncallback audit rows with lexical truth:")
    for row_id, truth, action, name in callback_labelled:
        print(f"  {row_id} action={action} truth={truth} name={name}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
