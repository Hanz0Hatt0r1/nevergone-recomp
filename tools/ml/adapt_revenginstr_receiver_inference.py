#!/usr/bin/env python3
"""Adapt project-neutral revenginstr receiver inference to Nevergone v5 evidence format.

The generic algorithm remains owned by revenginstr. This adapter only preserves
the historical Nevergone format identifier/compatibility fields consumed by the
existing ProductionClassifier/5 evidence pipeline.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

INPUT_FORMAT = "REVENGINSTR.ReceiverInference/1"
OUTPUT_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
MASS_FORMAT = "NEVERGONE.MassReceiverVtableInference/1"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            if row.get("format") != INPUT_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {INPUT_FORMAT}")
            rows.append(row)
    return rows


def adapt(row: dict[str, Any]) -> dict[str, Any]:
    out = dict(row)
    out["format"] = OUTPUT_FORMAT
    out["mass_inference_format"] = MASS_FORMAT
    out.setdefault("runtime_validation_status", "runtime_unobserved")
    out.setdefault("parameter_provenance_present", False)
    out.setdefault("structural_parameter_provenance_present", False)
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    rows = [adapt(row) for row in read_jsonl(args.input)]
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")
    print("input format:", INPUT_FORMAT)
    print("output format:", OUTPUT_FORMAT)
    print("rows:", len(rows))
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
