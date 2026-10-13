#!/usr/bin/env python3
"""Hierarchical production routing for Nevergone function analysis.

Pipeline:
    objective origin -> code_domain -> subsystem

External imports and linker runtime scaffolding are routed deterministically and
never reach semantic ML. Internal functions always reach code_domain. The
subsystem model is invoked only when code_domain predicts ``game`` with action
``accept``. Subsystem classes can additionally be blocked from automatic accept
at the hierarchy level; ``combat`` is blocked by default until independently
validated for production acceptance.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path
from typing import Any

from nevergone_function_origin import classify_function_origin

FORMAT = "NEVERGONE.HierarchicalFunctionRouter/1"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            rows.append(row)
    return rows


def record_id(row: dict[str, Any], index: int) -> str:
    value = row.get("id")
    if isinstance(value, str) and value:
        return value
    program = row.get("program")
    entry = row.get("source_entry")
    if isinstance(program, str) and isinstance(entry, str):
        return f"{program}:{entry}"
    return f"row-{index + 1}"


def run_classifier(
    model_dir: Path,
    rows: list[dict[str, Any]],
    *,
    expected_task: str,
    no_probabilities: bool,
) -> list[dict[str, Any]]:
    if not rows:
        return []
    with tempfile.TemporaryDirectory(prefix=f"nevergone-{expected_task}-") as temporary:
        root = Path(temporary)
        input_path = root / "input.jsonl"
        output_path = root / "output.jsonl"
        with input_path.open("w", encoding="utf-8") as handle:
            for row in rows:
                handle.write(json.dumps(row, sort_keys=True) + "\n")

        classifier = Path(__file__).with_name("classify_nevergone_pcode.py")
        command = [
            sys.executable,
            str(classifier),
            str(model_dir),
            str(input_path),
            "--output",
            str(output_path),
        ]
        if no_probabilities:
            command.append("--no-probabilities")
        subprocess.run(command, check=True)

        classified = read_jsonl(output_path)
        if len(classified) != len(rows):
            raise RuntimeError(
                f"{expected_task} classifier row count mismatch: "
                f"expected {len(rows)}, got {len(classified)}"
            )
        for result in classified:
            if result.get("task") != expected_task:
                raise RuntimeError(
                    f"expected {expected_task!r} classifier artifact, "
                    f"got task={result.get('task')!r}"
                )
        return classified


def deterministic_origin_result(row: dict[str, Any], index: int, origin: str) -> dict[str, Any]:
    return {
        "format": FORMAT,
        "id": record_id(row, index),
        "function_origin": {
            "prediction": origin,
            "confidence": 1.0,
            "action": "accept",
            "source": "deterministic_metadata",
        },
        "code_domain": None,
        "subsystem": None,
        "final_action": "accept",
        "routing_reason": "deterministic_source_origin",
        "semantic_models_invoked": [],
    }


def internal_base_result(
    row: dict[str, Any],
    index: int,
    code_domain: dict[str, Any],
) -> dict[str, Any]:
    prediction = code_domain.get("prediction")
    action = code_domain.get("action")
    if not isinstance(prediction, str) or action not in {"accept", "review", "escalate"}:
        raise ValueError("invalid code_domain classifier result")

    if prediction == "game" and action == "accept":
        reason = "subsystem_pending"
    elif prediction == "game":
        reason = "code_domain_game_not_accepted"
    else:
        reason = "code_domain_terminal"

    return {
        "format": FORMAT,
        "id": record_id(row, index),
        "function_origin": {
            "prediction": "internal",
            "confidence": 1.0,
            "action": "accept",
            "source": "deterministic_metadata",
        },
        "code_domain": dict(code_domain),
        "subsystem": None,
        "final_action": action,
        "routing_reason": reason,
        "semantic_models_invoked": ["code_domain"],
    }


def attach_subsystem(
    result: dict[str, Any],
    subsystem: dict[str, Any],
    *,
    blocked_accept_labels: set[str],
) -> None:
    prediction = subsystem.get("prediction")
    action = subsystem.get("action")
    if not isinstance(prediction, str) or action not in {"accept", "review", "escalate"}:
        raise ValueError("invalid subsystem classifier result")

    routed = dict(subsystem)
    if action == "accept" and prediction in blocked_accept_labels:
        routed["model_action"] = "accept"
        routed["action"] = "review"
        routed["production_guard"] = "subsystem_accept_blocked"
        action = "review"

    result["subsystem"] = routed
    result["final_action"] = action
    result["routing_reason"] = "subsystem_terminal"
    invoked = result.setdefault("semantic_models_invoked", [])
    if "subsystem" not in invoked:
        invoked.append("subsystem")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("code_domain_model", type=Path)
    parser.add_argument("subsystem_model", type=Path)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--no-probabilities", action="store_true")
    parser.add_argument(
        "--block-subsystem-accept",
        nargs="*",
        default=["combat"],
        metavar="LABEL",
        help="subsystem labels that may never produce final accept (default: combat)",
    )
    args = parser.parse_args()

    rows = read_jsonl(args.input)
    results: list[dict[str, Any] | None] = [None] * len(rows)
    internal_rows: list[dict[str, Any]] = []
    internal_indices: list[int] = []

    for index, row in enumerate(rows):
        origin = classify_function_origin(row.get("source_name"))
        if origin == "internal":
            internal_rows.append(row)
            internal_indices.append(index)
        else:
            results[index] = deterministic_origin_result(row, index, origin)

    code_results = run_classifier(
        args.code_domain_model,
        internal_rows,
        expected_task="code_domain",
        no_probabilities=args.no_probabilities,
    )

    subsystem_rows: list[dict[str, Any]] = []
    subsystem_result_indices: list[int] = []
    for row, original_index, code_result in zip(internal_rows, internal_indices, code_results):
        routed = internal_base_result(row, original_index, code_result)
        results[original_index] = routed
        if code_result.get("prediction") == "game" and code_result.get("action") == "accept":
            subsystem_rows.append(row)
            subsystem_result_indices.append(original_index)

    subsystem_results = run_classifier(
        args.subsystem_model,
        subsystem_rows,
        expected_task="subsystem",
        no_probabilities=args.no_probabilities,
    )
    blocked = set(args.block_subsystem_accept)
    for original_index, subsystem_result in zip(subsystem_result_indices, subsystem_results):
        routed = results[original_index]
        if routed is None:
            raise RuntimeError("missing code-domain result before subsystem routing")
        attach_subsystem(routed, subsystem_result, blocked_accept_labels=blocked)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for result in results:
            if result is None:
                raise RuntimeError("missing hierarchical result")
            handle.write(json.dumps(result, sort_keys=True) + "\n")

    completed = [result for result in results if result is not None]
    origin_counts = Counter(result["function_origin"]["prediction"] for result in completed)
    final_counts = Counter(result["final_action"] for result in completed)
    code_counts = Counter(
        result["code_domain"]["prediction"]
        for result in completed
        if isinstance(result.get("code_domain"), dict)
    )
    subsystem_counts = Counter(
        result["subsystem"]["prediction"]
        for result in completed
        if isinstance(result.get("subsystem"), dict)
    )
    subsystem_actions = Counter(
        result["subsystem"]["action"]
        for result in completed
        if isinstance(result.get("subsystem"), dict)
    )

    print("functions:", len(completed))
    print("origins:", origin_counts)
    print("code_domain invoked:", len(internal_indices), code_counts)
    print("subsystem invoked:", len(subsystem_result_indices), subsystem_counts)
    print("subsystem actions:", subsystem_actions)
    print("final actions:", final_counts)
    print("blocked subsystem accepts:", sorted(blocked))
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
