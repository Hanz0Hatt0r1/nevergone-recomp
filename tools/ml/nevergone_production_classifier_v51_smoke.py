#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        trace = tmp / "trace.jsonl"
        plan = tmp / "plan.tsv"
        receivers = tmp / "receivers.tsv"
        catalog = tmp / "catalog.jsonl"
        inference = tmp / "inference.jsonl"
        callgraph = tmp / "callgraph.jsonl"
        evidence = tmp / "evidence.jsonl"

        trace_rows = [
            {"id": "f1", "program": "libcocos2dcpp.so", "source_entry": "00100000"},
            {"id": "f2", "program": "libcocos2dcpp.so", "source_entry": "00100100"},
            {"id": "f3", "program": "libcocos2dcpp.so", "source_entry": "00100200"},
            {"id": "m1", "program": "libffmpeg.so", "source_entry": "00010000"},
        ]
        write_jsonl(trace, trace_rows)

        subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/ml/build_nevergone_mass_receiver_plan.py"),
                str(trace),
                str(plan),
            ],
            check=True,
        )
        fallback_plan = tmp / "fallback-plan.tsv"
        fallback_env = os.environ.copy()
        fallback_env["REVENGINSTR_REPO"] = str(tmp / "missing-revenginstr")
        subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/ml/build_nevergone_mass_receiver_plan.py"),
                str(trace),
                str(fallback_plan),
            ],
            check=True,
            env=fallback_env,
        )
        assert fallback_plan.read_bytes() == plan.read_bytes()
        plan_rows = list(csv.DictReader(plan.open(encoding="utf-8"), delimiter="\t"))
        assert [row["source_id"] for row in plan_rows] == [
            "libcocos2dcpp.so:00100000",
            "libcocos2dcpp.so:00100100",
            "libcocos2dcpp.so:00100200",
        ], plan_rows
        assert all(row["expected_calls"] == "" for row in plan_rows), plan_rows

        fields = [
            "format", "source_id", "function_entry", "call_index", "call_address", "call_time",
            "arg_count", "receiver_present", "receiver_storage_kind", "receiver_token",
            "receiver_type_name", "receiver_type_display", "receiver_pointer_depth",
            "pointee_type_name", "pointee_type_display", "flow_type_node_count",
            "flow_single_pointer_pointee_names_json", "flow_single_pointer_pointee_displays_json",
        ]
        receiver_rows = [
            {
                "format": "NEVERGONE.GhidraIndirectReceiverType/2",
                "source_id": "libcocos2dcpp.so:00100000",
                "function_entry": "00100000",
                "call_index": "0",
                "call_address": "00100020",
                "call_time": "0",
                "arg_count": "1",
                "receiver_present": "1",
                "receiver_storage_kind": "unique",
                "receiver_token": "def:CAST@00100010:0:4",
                "receiver_type_name": "GameThing *",
                "receiver_type_display": "GameThing *",
                "receiver_pointer_depth": "1",
                "pointee_type_name": "GameThing",
                "pointee_type_display": "GameThing",
                "flow_type_node_count": "1",
                "flow_single_pointer_pointee_names_json": "[]",
                "flow_single_pointer_pointee_displays_json": "[]",
            },
            {
                "format": "NEVERGONE.GhidraIndirectReceiverType/2",
                "source_id": "libcocos2dcpp.so:00100100",
                "function_entry": "00100100",
                "call_index": "0",
                "call_address": "00100120",
                "call_time": "0",
                "arg_count": "1",
                "receiver_present": "1",
                "receiver_storage_kind": "unique",
                "receiver_token": "def:COPY@00100110:0:4",
                "receiver_type_name": "void **",
                "receiver_type_display": "void **",
                "receiver_pointer_depth": "2",
                "pointee_type_name": "void *",
                "pointee_type_display": "void *",
                "flow_type_node_count": "2",
                "flow_single_pointer_pointee_names_json": json.dumps(["EngineThing"]),
                "flow_single_pointer_pointee_displays_json": json.dumps(["EngineThing"]),
            },
        ]
        with receivers.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t")
            writer.writeheader()
            writer.writerows(receiver_rows)

        write_jsonl(
            catalog,
            [
                {
                    "format": "NEVERGONE.StaticVtableSlotCatalog/3",
                    "class_name": "GameThing",
                    "receiver_vtable_id": "libcocos2dcpp.so:00900000",
                },
                {
                    "format": "NEVERGONE.StaticVtableSlotCatalog/3",
                    "class_name": "EngineThing",
                    "receiver_vtable_id": "libcocos2dcpp.so:00900100",
                },
            ],
        )

        subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/ml/infer_nevergone_mass_receiver_vtables.py"),
                str(receivers),
                str(catalog),
                "--output",
                str(inference),
            ],
            check=True,
        )
        inferred = [json.loads(line) for line in inference.read_text().splitlines() if line.strip()]
        assert len(inferred) == 2, inferred
        assert all(row["status"] == "resolved_unique_vtable" for row in inferred), inferred
        assert all(row["dispatch_slot_offset"] is None for row in inferred), inferred
        assert all(row["slot_evidence_present"] is False for row in inferred), inferred
        assert {row["class_evidence_source"] for row in inferred} == {
            "direct_receiver_type", "transparent_type_flow"
        }, inferred

        write_jsonl(
            callgraph,
            [
                {"id": row["id"], "metrics": {"fan_in": i, "fan_out": i + 1}}
                for i, row in enumerate(trace_rows)
            ],
        )

        audit = subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/ml/audit_nevergone_v51_evidence_coverage.py"),
                str(trace),
                "--callgraph-metrics",
                str(callgraph),
                "--receiver-types",
                str(receivers),
                "--receiver-inference",
                str(inference),
            ],
            check=True,
            text=True,
            capture_output=True,
        )
        assert "receiver eligible functions: 3" in audit.stdout, audit.stdout
        assert '"matched_functions": 2' in audit.stdout, audit.stdout
        assert '"eligible_function_coverage": 0.666667' in audit.stdout, audit.stdout

        subprocess.run(
            [
                sys.executable,
                str(ROOT / "tools/ml/build_nevergone_v5_evidence.py"),
                str(trace),
                "--callgraph-metrics",
                str(callgraph),
                "--receiver-types",
                str(receivers),
                "--receiver-inference",
                str(inference),
                "--output",
                str(evidence),
            ],
            check=True,
        )
        evidence_text = evidence.read_text(encoding="utf-8")
        assert "GameThing" not in evidence_text, evidence_text
        assert "EngineThing" not in evidence_text, evidence_text
        evidence_rows = {row["id"]: row for row in map(json.loads, evidence_text.splitlines())}
        assert evidence_rows["f1"]["features"]["receiver.resolved_count"] == 1.0
        assert evidence_rows["f2"]["features"]["receiver.evidence_transparent_type_flow_count"] == 1.0
        assert evidence_rows["f3"]["features"].get("has_receiver_types", 0.0) == 0.0

    print("ProductionClassifier/5.1 mass receiver evidence smoke: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
