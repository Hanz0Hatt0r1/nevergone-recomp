#!/usr/bin/env python3
"""Route objective function origins before Nevergone semantic classification.

Rows identified from source metadata as external imports or linker runtime
scaffolding are accepted by a deterministic origin router and never sent to the
code-domain ML model. Internal rows are delegated unchanged to
`classify_nevergone_pcode.py`.
"""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

from nevergone_function_origin import classify_function_origin

ROUTER_FORMAT = "NEVERGONE.FunctionRouter/1"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
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


def deterministic_result(row: dict[str, Any], index: int, origin: str) -> dict[str, Any]:
    return {
        "format": ROUTER_FORMAT,
        "id": record_id(row, index),
        "task": "function_origin",
        "prediction": origin,
        "function_origin": origin,
        "confidence": 1.0,
        "action": "accept",
        "routing_reason": "deterministic_source_origin",
        "semantic_model_invoked": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model_dir", type=Path)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--no-probabilities", action="store_true")
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
            results[index] = deterministic_result(row, index, origin)

    if internal_rows:
        with tempfile.TemporaryDirectory(prefix="nevergone-origin-router-") as temp:
            temp_dir = Path(temp)
            internal_input = temp_dir / "internal.jsonl"
            internal_output = temp_dir / "classified.jsonl"
            with internal_input.open("w", encoding="utf-8") as handle:
                for row in internal_rows:
                    handle.write(json.dumps(row, sort_keys=True) + "\n")

            classifier = Path(__file__).with_name("classify_nevergone_pcode.py")
            command = [
                sys.executable,
                str(classifier),
                str(args.model_dir),
                str(internal_input),
                "--output",
                str(internal_output),
            ]
            if args.no_probabilities:
                command.append("--no-probabilities")
            subprocess.run(command, check=True)

            classified = read_jsonl(internal_output)
            if len(classified) != len(internal_indices):
                raise RuntimeError("semantic classifier row count mismatch")
            for output_row, original_index in zip(classified, internal_indices):
                enriched = dict(output_row)
                enriched["function_origin"] = "internal"
                enriched["semantic_model_invoked"] = True
                enriched["routing_reason"] = "semantic_model"
                results[original_index] = enriched

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for result in results:
            if result is None:
                raise RuntimeError("missing routed result")
            handle.write(json.dumps(result, sort_keys=True) + "\n")

    counts: dict[str, int] = {}
    for result in results:
        assert result is not None
        origin = str(result.get("function_origin", "unknown"))
        counts[origin] = counts.get(origin, 0) + 1
    print("functions:", len(results))
    print("origins:", counts)
    print("semantic_model_invoked:", len(internal_indices))
    print("deterministic_routed:", len(results) - len(internal_indices))
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
