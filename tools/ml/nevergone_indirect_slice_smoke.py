#!/usr/bin/env python3
"""Dependency-light smoke test for bounded CALLIND slice planning and summaries."""
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
    repo = Path(__file__).resolve().parents[2]
    builder = repo / "tools/ml/build_nevergone_indirect_slice_plan.py"
    summarizer = repo / "tools/ml/summarize_nevergone_indirect_call_slices.py"

    with tempfile.TemporaryDirectory() as td:
        root = Path(td)
        analysis = root / "analysis.jsonl"
        plan = root / "plan"
        evidence = root / "evidence"
        output = root / "output.jsonl"
        rows = [
            {
                "id": "libcocos2dcpp.so:001000",
                "final_action": "review",
                "indirect_call_pattern_evidence": {
                    "present": True,
                    "call_count": 2,
                    "depth_limited_expression_count": 1,
                    "depth_limited_fraction": 0.5,
                    "nesting_depth_max": 6,
                },
            },
            {
                "id": "libcocos2dcpp.so:002000",
                "final_action": "accept",
                "indirect_call_pattern_evidence": {
                    "present": True,
                    "call_count": 1,
                    "depth_limited_expression_count": 0,
                    "depth_limited_fraction": 0.0,
                    "nesting_depth_max": 4,
                },
            },
        ]
        analysis.write_text("".join(json.dumps(row) + "\n" for row in rows), encoding="utf-8")

        subprocess.run(
            [
                sys.executable,
                str(builder),
                str(analysis),
                "--output-dir",
                str(plan),
                "--max-nodes",
                "96",
                "--max-depth",
                "24",
            ],
            check=True,
        )
        manifest = json.loads((plan / "manifest.json").read_text(encoding="utf-8"))
        assert manifest["selected_functions"] == 1, manifest
        assert manifest["selected_calls"] == 2, manifest
        assert manifest["selected_depth_limited_calls"] == 1, manifest
        runner = (plan / "run_indirect_call_slices.sh").read_text(encoding="utf-8")
        assert "ExportIndirectCallSlice.java" in runner
        assert 'MAX_NODES:=96' in runner
        assert 'MAX_DEPTH:=24' in runner

        fields = [
            "row_type",
            "function_entry",
            "call_index",
            "call_address",
            "call_time",
            "arg_count",
            "node_id",
            "parent_node_id",
            "input_index",
            "depth",
            "node_token",
            "constant_value",
            "defining_opcode",
            "defining_address",
            "defining_time",
            "truncation",
            "node_count",
            "truncated_by_budget",
            "depth_truncation_count",
            "cycle_count",
            "max_nodes",
            "max_depth",
        ]
        common = {
            "function_entry": "001000",
            "call_index": 0,
            "call_address": "001100",
            "call_time": 0,
            "arg_count": 2,
            "max_nodes": 96,
            "max_depth": 24,
        }
        slice_rows = [
            {
                **common,
                "row_type": "summary",
                "node_id": "-",
                "parent_node_id": "-",
                "input_index": "-",
                "depth": "-",
                "node_token": "-",
                "constant_value": "-",
                "defining_opcode": "-",
                "defining_address": "-",
                "defining_time": "-",
                "truncation": "-",
                "node_count": 5,
                "truncated_by_budget": 0,
                "depth_truncation_count": 0,
                "cycle_count": 0,
            },
            {
                **common,
                "row_type": "node",
                "node_id": 0,
                "parent_node_id": -1,
                "input_index": -1,
                "depth": 0,
                "node_token": "def:LOAD@001100:0:4",
                "constant_value": "-",
                "defining_opcode": "LOAD",
                "defining_address": "001100",
                "defining_time": 0,
                "truncation": "-",
                "node_count": "-",
                "truncated_by_budget": "-",
                "depth_truncation_count": "-",
                "cycle_count": "-",
            },
            {
                **common,
                "row_type": "node",
                "node_id": 1,
                "parent_node_id": 0,
                "input_index": 1,
                "depth": 1,
                "node_token": "def:INT_ADD@0010fc:1:4",
                "constant_value": "-",
                "defining_opcode": "INT_ADD",
                "defining_address": "0010fc",
                "defining_time": 1,
                "truncation": "-",
                "node_count": "-",
                "truncated_by_budget": "-",
                "depth_truncation_count": "-",
                "cycle_count": "-",
            },
            {
                **common,
                "row_type": "node",
                "node_id": 2,
                "parent_node_id": 1,
                "input_index": 0,
                "depth": 2,
                "node_token": "def:LOAD@0010f8:2:4",
                "constant_value": "-",
                "defining_opcode": "LOAD",
                "defining_address": "0010f8",
                "defining_time": 2,
                "truncation": "-",
                "node_count": "-",
                "truncated_by_budget": "-",
                "depth_truncation_count": "-",
                "cycle_count": "-",
            },
            {
                **common,
                "row_type": "node",
                "node_id": 3,
                "parent_node_id": 1,
                "input_index": 1,
                "depth": 2,
                "node_token": "const:0x80:4",
                "constant_value": "0x80",
                "defining_opcode": "-",
                "defining_address": "-",
                "defining_time": -1,
                "truncation": "-",
                "node_count": "-",
                "truncated_by_budget": "-",
                "depth_truncation_count": "-",
                "cycle_count": "-",
            },
            {
                **common,
                "row_type": "node",
                "node_id": 4,
                "parent_node_id": 2,
                "input_index": 1,
                "depth": 3,
                "node_token": "register:r0:4",
                "constant_value": "-",
                "defining_opcode": "-",
                "defining_address": "-",
                "defining_time": -1,
                "truncation": "-",
                "node_count": "-",
                "truncated_by_budget": "-",
                "depth_truncation_count": "-",
                "cycle_count": "-",
            },
        ]
        write_tsv(
            evidence / "indirect-slices/libcocos2dcpp.so_001000.tsv",
            fields,
            slice_rows,
        )

        subprocess.run(
            [
                sys.executable,
                str(summarizer),
                str(analysis),
                str(evidence),
                "--output",
                str(output),
            ],
            check=True,
        )
        out_rows = [json.loads(line) for line in output.read_text(encoding="utf-8").splitlines()]
        first = out_rows[0]
        ev = first["indirect_call_slice_evidence"]
        assert first["final_action"] == "review"
        assert ev["present"] is True
        assert ev["complete_call_count"] == 1
        assert ev["nested_load_call_count"] == 1
        assert ev["primary_slot_offset_counts"] == {"0x80": 1}, ev
        call = ev["calls"][0]
        assert call["primary_slot_offset_candidate"] == "0x80", call
        assert call["root_opcode"] == "LOAD", call
        assert call["slot_signature"].endswith("@0x80"), call
        assert out_rows[1]["final_action"] == "accept"
        assert out_rows[1]["indirect_call_slice_evidence"]["present"] is False

    print("nevergone indirect slice smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
