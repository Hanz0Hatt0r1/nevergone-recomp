#!/usr/bin/env python3
"""Dependency-light smoke test for analysis evidence ingestion."""
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path


def write_tsv(path: Path, fieldnames: list[str], rows: list[dict[str, object]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, delimiter="\t")
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        analysis = root / "analysis.jsonl"
        evidence = root / "evidence"
        output = root / "output.jsonl"

        row = {
            "id": "libcocos2dcpp.so:001000",
            "final_action": "review",
            "structural_metrics": {
                "op_count": 256,
                "definition_coverage_proxy": 0.9,
                "indirect_call_count": 1,
            },
            "suggested_analysis_method": {
                "primary": "expand_pcode",
                "recommendations": [
                    {"method": "expand_pcode", "score": 100, "reasons": ["cap"]},
                    {"method": "resolve_indirect_calls", "score": 96, "reasons": ["callind"]},
                ],
            },
        }
        analysis.write_text(json.dumps(row) + "\n", encoding="utf-8")
        stem = "libcocos2dcpp.so_001000.tsv"

        expanded_fields = [
            "function_entry",
            "op_address",
            "op_time",
            "opcode",
            "output",
            "inputs",
            "callee_entry",
            "callee_name",
        ]
        expanded_rows = [
            {
                "function_entry": "001000",
                "op_address": "001000",
                "op_time": 0,
                "opcode": "LOAD",
                "output": "def:001000:0:4",
                "inputs": "register:r0:4",
                "callee_entry": "-",
                "callee_name": "-",
            },
            {
                "function_entry": "001000",
                "op_address": "001004",
                "op_time": 0,
                "opcode": "CALL",
                "output": "-",
                "inputs": "register:r0:4",
                "callee_entry": "002000",
                "callee_name": "callee",
            },
            {
                "function_entry": "001000",
                "op_address": "001008",
                "op_time": 0,
                "opcode": "CALLIND",
                "output": "-",
                "inputs": "register:r0:4",
                "callee_entry": "-",
                "callee_name": "-",
            },
            {
                "function_entry": "001000",
                "op_address": "00100c",
                "op_time": 0,
                "opcode": "CBRANCH",
                "output": "-",
                "inputs": "register:r1:1",
                "callee_entry": "-",
                "callee_name": "-",
            },
        ]
        write_tsv(evidence / "expanded-pcode" / stem, expanded_fields, expanded_rows)

        indirect_fields = [
            "function_entry",
            "call_address",
            "call_time",
            "target",
            "target_expression",
            "arg_count",
        ]
        indirect_rows = [
            {
                "function_entry": "001000",
                "call_address": "001008",
                "call_time": 0,
                "target": "def:LOAD@001000:0:4",
                "target_expression": "LOAD(PTRADD(register:r4:4,const:0x8:4))",
                "arg_count": 3,
            }
        ]
        write_tsv(evidence / "indirect-calls" / stem, indirect_fields, indirect_rows)

        tool = Path(__file__).with_name("ingest_nevergone_analysis_evidence.py")
        result = subprocess.run(
            [sys.executable, str(tool), str(analysis), str(evidence), "--output", str(output)],
            check=True,
            capture_output=True,
            text=True,
        )
        assert "final_action unchanged: yes" in result.stdout

        rows = [json.loads(line) for line in output.read_text(encoding="utf-8").splitlines() if line.strip()]
        assert len(rows) == 1
        out = rows[0]
        assert out["final_action"] == "review"

        expanded = out["expanded_pcode_evidence"]
        assert expanded["present"] is True
        assert expanded["requested"] is True
        assert expanded["op_count"] == 4
        assert expanded["call_count"] == 2
        assert expanded["direct_call_count"] == 1
        assert expanded["indirect_call_count"] == 1
        assert expanded["resolved_direct_call_count"] == 1
        assert expanded["decision_point_count"] == 1
        assert expanded["portable_was_capped"] is True
        assert expanded["expansion_delta_ops"] == -252

        indirect = out["indirect_call_evidence"]
        assert indirect["present"] is True
        assert indirect["requested"] is True
        assert indirect["call_count"] == 1
        assert indirect["expression_root_counts"] == {"LOAD": 1}
        assert indirect["expression_class_counts"] == {"load_target": 1}
        assert indirect["arg_count_median"] == 3
        assert indirect["evidence_delta_vs_portable"] == 0

    print("analysis evidence ingest smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
