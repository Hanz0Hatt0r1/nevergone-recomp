#!/usr/bin/env python3
"""Adapt REVENGINSTR.Evidence/1 rows to Nevergone ProductionEvidence/1.

The feature dictionary is copied byte-for-byte at the JSON-value level. This
adapter changes only the outer format identifier so historical Nevergone model
training/inference contracts can consume evidence produced by the generic core.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

INPUT_FORMAT = "REVENGINSTR.Evidence/1"
OUTPUT_FORMAT = "NEVERGONE.ProductionEvidence/1"


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
            if not isinstance(row.get("id"), str) or not isinstance(row.get("features"), dict):
                raise ValueError(f"{path}:{line_no}: missing id/features")
            rows.append(row)
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    rows = read_jsonl(args.input)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps({
                "format": OUTPUT_FORMAT,
                "id": row["id"],
                "features": row["features"],
            }, sort_keys=True) + "\n")
    print("input format:", INPUT_FORMAT)
    print("output format:", OUTPUT_FORMAT)
    print("rows:", len(rows))
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
