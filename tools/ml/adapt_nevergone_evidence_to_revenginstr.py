#!/usr/bin/env python3
"""Convert historical Nevergone ProductionEvidence/1 to REVENGINSTR.Evidence/1.

This is a format-boundary adapter only. Feature names and numeric values are
preserved exactly; no Nevergone semantic identity is added or reinterpreted.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

SOURCE_FORMAT = "NEVERGONE.ProductionEvidence/1"
TARGET_FORMAT = "REVENGINSTR.Evidence/1"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()

    args.output.parent.mkdir(parents=True, exist_ok=True)
    count = 0
    with args.input.open("r", encoding="utf-8") as source, args.output.open("w", encoding="utf-8") as target:
        for line_no, raw in enumerate(source, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict) or row.get("format") != SOURCE_FORMAT:
                raise ValueError(f"{args.input}:{line_no}: expected {SOURCE_FORMAT}")
            record_id = row.get("id")
            features = row.get("features")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError(f"{args.input}:{line_no}: missing id")
            if not isinstance(features, dict):
                raise ValueError(f"{args.input}:{line_no}: features must be object")
            target.write(json.dumps({"format": TARGET_FORMAT, "id": record_id, "features": features}, sort_keys=True) + "\n")
            count += 1

    print("format:", TARGET_FORMAT)
    print("rows:", count)
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
