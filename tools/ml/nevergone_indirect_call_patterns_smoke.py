#!/usr/bin/env python3
"""Dependency-light smoke test for indirect-call target pattern analysis."""
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def write_tsv(path: Path, rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    fieldnames = ["function_entry", "call_address", "call_time", "target", "target_expression", "arg_count"]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        analysis = root / "analysis.jsonl"
        evidence = root / "evidence"
        output = root / "output.jsonl"
        record_id = "libcocos2dcpp.so:001000"
        row = {
            "id": record_id,
            "final_action": "review",
            "suggested_analysis_method": {
                "primary": "resolve_indirect_calls",
                "recommendations": [
                    {"method": "resolve_indirect_calls", "score": 100, "reasons": ["synthetic"]}
                ],
            },
        }
        analysis.write_text(json.dumps(row) + "\n", encoding="utf-8")

        write_tsv(
            evidence / "indirect-calls" / "libcocos2dcpp.so_001000.tsv",
            [
                {
                    "function_entry": "001000",
                    "call_address": "001020",
                    "call_time": 0,
                    "target": "def:LOAD@001020:0:4",
                    "target_expression": "LOAD(PTRADD(LOAD(register:r0:4),const:0x18:4))",
                    "arg_count": 2,
                },
                {
                    "function_entry": "001000",
                    "call_address": "001030",
                    "call_time": 0,
                    "target": "def:LOAD@001030:0:4",
                    "target_expression": "LOAD(MULTIEQUAL(register:r1:4,register:r2:4)){depth-limit}",
                    "arg_count": 1,
                },
                {
                    "function_entry": "001000",
                    "call_address": "001040",
                    "call_time": 0,
                    "target": "const:0x1234:4",
                    "target_expression": "const:0x1234:4",
                    "arg_count": 0,
                },
            ],
        )

        script = Path(__file__).with_name("analyze_nevergone_indirect_call_patterns.py")
        result = subprocess.run(
            [sys.executable, str(script), str(analysis), str(evidence), "--output", str(output)],
            check=True,
            capture_output=True,
            text=True,
        )
        if "final_action unchanged: yes" not in result.stdout:
            raise AssertionError(result.stdout)

        parsed = json.loads(output.read_text(encoding="utf-8").strip())
        if parsed["final_action"] != "review":
            raise AssertionError("final_action changed")
        ev = parsed["indirect_call_pattern_evidence"]
        if ev["call_count"] != 3:
            raise AssertionError(ev)
        if ev["pattern_counts"].get("load_from_address_arithmetic") != 1:
            raise AssertionError(ev)
        if ev["pattern_counts"].get("load_from_phi_value") != 1:
            raise AssertionError(ev)
        if ev["pattern_counts"].get("constant_target") != 1:
            raise AssertionError(ev)
        if ev["nested_load_expression_count"] != 1:
            raise AssertionError(ev)
        if ev["depth_limited_expression_count"] != 1:
            raise AssertionError(ev)
        if ev["small_constant_counts"].get("0x18") != 1:
            raise AssertionError(ev)

    print("nevergone indirect call patterns smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
