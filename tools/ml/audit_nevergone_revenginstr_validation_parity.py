#!/usr/bin/env python3
"""Compare historical Nevergone frozen-drift output with generic revenginstr output.

Format identifiers and explanatory note text are intentionally different. All
retained diagnostic values must match within tolerance before the historical
Nevergone drift implementation can be treated as migrated.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.RevenginstrValidationParity/1"
OLD_FORMAT = "NEVERGONE.FrozenDriftAuditV10/1"
GENERIC_FORMAT = "REVENGINSTR.DriftAudit/1"
IGNORED_TOP_LEVEL = {"format", "note"}


def compare_values(left: Any, right: Any, path: str, tolerance: float, mismatches: list[dict[str, Any]]) -> None:
    if isinstance(left, bool) or isinstance(right, bool):
        if left is not right:
            mismatches.append({"path": path, "historical": left, "generic": right})
        return
    if isinstance(left, (int, float)) and isinstance(right, (int, float)):
        a = float(left)
        b = float(right)
        if not (math.isfinite(a) and math.isfinite(b)):
            if not (math.isnan(a) and math.isnan(b)) and a != b:
                mismatches.append({"path": path, "historical": left, "generic": right})
            return
        if abs(a - b) > tolerance:
            mismatches.append(
                {
                    "path": path,
                    "historical": left,
                    "generic": right,
                    "abs_delta": abs(a - b),
                }
            )
        return
    if isinstance(left, dict) and isinstance(right, dict):
        left_keys = set(left)
        right_keys = set(right)
        if path == "$":
            left_keys -= IGNORED_TOP_LEVEL
            right_keys -= IGNORED_TOP_LEVEL
        if left_keys != right_keys:
            mismatches.append(
                {
                    "path": path,
                    "reason": "key_set_mismatch",
                    "historical_only": sorted(left_keys - right_keys),
                    "generic_only": sorted(right_keys - left_keys),
                }
            )
        for key in sorted(left_keys & right_keys):
            compare_values(left[key], right[key], f"{path}.{key}", tolerance, mismatches)
        return
    if isinstance(left, list) and isinstance(right, list):
        if len(left) != len(right):
            mismatches.append(
                {"path": path, "reason": "length_mismatch", "historical": len(left), "generic": len(right)}
            )
            return
        for index, (old, new) in enumerate(zip(left, right)):
            compare_values(old, new, f"{path}[{index}]", tolerance, mismatches)
        return
    if left != right:
        mismatches.append({"path": path, "historical": left, "generic": right})


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("historical", type=Path)
    parser.add_argument("generic", type=Path)
    parser.add_argument("--tolerance", type=float, default=1e-12)
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()

    historical = json.loads(args.historical.read_text(encoding="utf-8"))
    generic = json.loads(args.generic.read_text(encoding="utf-8"))
    if historical.get("format") != OLD_FORMAT:
        raise ValueError(f"historical report must be {OLD_FORMAT}")
    if generic.get("format") != GENERIC_FORMAT:
        raise ValueError(f"generic report must be {GENERIC_FORMAT}")

    mismatches: list[dict[str, Any]] = []
    compare_values(historical, generic, "$", args.tolerance, mismatches)
    report = {
        "format": FORMAT,
        "historical_format": OLD_FORMAT,
        "generic_format": GENERIC_FORMAT,
        "tolerance": args.tolerance,
        "mismatch_count": len(mismatches),
        "parity_pass": not mismatches,
        "mismatches": mismatches[:50],
    }
    if args.output is not None:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", FORMAT)
    print("historical:", OLD_FORMAT)
    print("generic:", GENERIC_FORMAT)
    print("tolerance:", args.tolerance)
    print("mismatches:", len(mismatches))
    print("validation parity pass:", not mismatches)
    if mismatches:
        print("first mismatch:", mismatches[0])
    if args.output is not None:
        print("output:", args.output)
    if args.strict and mismatches:
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
