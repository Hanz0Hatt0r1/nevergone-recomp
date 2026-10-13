#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def analysis_row(source_id: str, slots: list[str]) -> dict:
    return {
        "id": source_id,
        "final_action": "review",
        "indirect_call_slice_evidence": {
            "present": True,
            "calls": [
                {
                    "call_index": i,
                    "call_address": f"00{i+1:06x}",
                    "primary_slot_offset_candidate": slot,
                }
                for i, slot in enumerate(slots)
            ],
        },
    }


def catalog_row(class_name: str, vtable: str, slot: str, target: str, runtime: str) -> dict:
    return {
        "format": "NEVERGONE.StaticVtableSlotCatalog/3",
        "class_name": class_name,
        "class_evidence_kind": "itanium_mangled_vtable",
        "vtable_symbol_name": "_ZTV" + class_name.replace("::", ""),
        "receiver_vtable_id": vtable,
        "dispatch_slot_offset": slot,
        "candidate_target_ids": [target],
        "runtime_validation_status": runtime,
        "runtime_target_ids": [target] if runtime == "runtime_match" else [],
        "static_status": "single_function_memory_pointer",
        "primary_prefix_slot_count": 12,
        "source_kind": "ghidra_program_memory_pointer",
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        analysis = tmp / "analysis.jsonl"
        receivers = tmp / "receivers.tsv"
        catalog = tmp / "catalog.jsonl"
        runtime = tmp / "runtime.jsonl"
        inference = tmp / "inference.jsonl"
        queries = tmp / "queries.jsonl"
        resolutions = tmp / "resolutions.jsonl"

        write_jsonl(analysis, [
            analysis_row("libcocos2dcpp.so:0029ec10", ["0x24", "0x8"]),
            analysis_row("libcocos2dcpp.so:0029ed0c", ["0x24", "0x8"]),
            analysis_row("libcocos2dcpp.so:00300000", ["0x10"]),
            analysis_row("libcocos2dcpp.so:00310000", ["0x10"]),
            analysis_row("libcocos2dcpp.so:00320000", ["0x10"]),
        ])

        fields = [
            "format", "source_id", "function_entry", "call_index", "call_address", "call_time",
            "arg_count", "receiver_present", "receiver_storage_kind", "receiver_token",
            "receiver_type_name", "receiver_type_display", "receiver_pointer_depth",
            "pointee_type_name", "pointee_type_display",
        ]
        receiver_rows = []
        for source_id, class_name, count in [
            ("libcocos2dcpp.so:0029ec10", "ActionData", 2),
            ("libcocos2dcpp.so:0029ed0c", "EenemyActionDataSet", 2),
            ("libcocos2dcpp.so:00300000", "cocos2d::CCNode", 1),
            ("libcocos2dcpp.so:00310000", "DupClass", 1),
            ("libcocos2dcpp.so:00320000", "void", 1),
        ]:
            for i in range(count):
                receiver_rows.append({
                    "format": "NEVERGONE.GhidraIndirectReceiverType/1",
                    "source_id": source_id,
                    "function_entry": source_id.split(":", 1)[1],
                    "call_index": str(i),
                    "call_address": f"00{i+1:06x}",
                    "call_time": "0",
                    "arg_count": "1",
                    "receiver_present": "1",
                    "receiver_storage_kind": "unique",
                    "receiver_token": "def:COPY@0:0:4",
                    "receiver_type_name": class_name.split("::")[-1] + " *",
                    "receiver_type_display": class_name + " *",
                    "receiver_pointer_depth": "1",
                    "pointee_type_name": class_name.split("::")[-1],
                    "pointee_type_display": class_name,
                })
        with receivers.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t")
            writer.writeheader()
            writer.writerows(receiver_rows)

        write_jsonl(catalog, [
            catalog_row("ActionData", "libcocos2dcpp.so:008f3928", "0x24", "libcocos2dcpp.so:0029e9ba", "runtime_match"),
            catalog_row("ActionData", "libcocos2dcpp.so:008f3928", "0x8", "libcocos2dcpp.so:0029ea5c", "runtime_match"),
            catalog_row("EenemyActionDataSet", "libcocos2dcpp.so:008f3958", "0x24", "libcocos2dcpp.so:0029eb4a", "runtime_match"),
            catalog_row("EenemyActionDataSet", "libcocos2dcpp.so:008f3958", "0x8", "libcocos2dcpp.so:0029eaec", "runtime_match"),
            catalog_row("cocos2d::CCNode", "libcocos2dcpp.so:00900008", "0x10", "libcocos2dcpp.so:00400000", "runtime_unobserved"),
            catalog_row("DupClass", "libcocos2dcpp.so:00910008", "0x10", "libcocos2dcpp.so:00410000", "runtime_unobserved"),
            catalog_row("DupClass", "libcocos2dcpp.so:00920008", "0x10", "libcocos2dcpp.so:00420000", "runtime_unobserved"),
        ])

        write_jsonl(runtime, [
            {"source_id": "libcocos2dcpp.so:0029ec10", "call_index": 0, "receiver_vtable_id": "libcocos2dcpp.so:008f3928", "dispatch_slot_offset": "0x24"},
            {"source_id": "libcocos2dcpp.so:0029ec10", "call_index": 1, "receiver_vtable_id": "libcocos2dcpp.so:008f3928", "dispatch_slot_offset": "0x8"},
            {"source_id": "libcocos2dcpp.so:0029ed0c", "call_index": 0, "receiver_vtable_id": "libcocos2dcpp.so:008f3958", "dispatch_slot_offset": "0x24"},
            {"source_id": "libcocos2dcpp.so:0029ed0c", "call_index": 1, "receiver_vtable_id": "libcocos2dcpp.so:008f3958", "dispatch_slot_offset": "0x8"},
        ])

        subprocess.run([
            sys.executable,
            str(ROOT / "tools/ml/infer_nevergone_static_receiver_vtables.py"),
            str(analysis), str(receivers), str(catalog),
            "--runtime-observations", str(runtime),
            "--output", str(inference),
            "--queries-output", str(queries),
        ], check=True)

        inferred = [json.loads(x) for x in inference.read_text().splitlines() if x.strip()]
        status_counts = Counter(x["status"] for x in inferred)
        assert status_counts["resolved_unique_vtable_runtime_match"] == 4, status_counts
        assert status_counts["resolved_unique_vtable"] == 1, status_counts
        assert status_counts["ambiguous_class_vtable"] == 1, status_counts
        assert status_counts["unresolved_receiver_class"] == 1, status_counts

        query_rows = [json.loads(x) for x in queries.read_text().splitlines() if x.strip()]
        assert len(query_rows) == 5, query_rows
        ccnode = next(x for x in query_rows if x["source_id"].endswith(":00300000"))
        assert ccnode["receiver_vtable_id"] == "libcocos2dcpp.so:00900008"
        assert ccnode["receiver_class_match_kind"] == "exact_catalog_class_name"

        subprocess.run([
            sys.executable,
            str(ROOT / "tools/ml/resolve_nevergone_receiver_vtable_slots.py"),
            str(catalog), str(queries), "--output", str(resolutions),
        ], check=True)
        resolved = [json.loads(x) for x in resolutions.read_text().splitlines() if x.strip()]
        resolution_counts = Counter(x["status"] for x in resolved)
        assert resolution_counts["exact_key_runtime_verified"] == 4, resolution_counts
        assert resolution_counts["exact_key_single_candidate"] == 1, resolution_counts

        java = (ROOT / "tools/ghidra/ExportIndirectCallReceiverTypes.java").read_text(encoding="utf-8")
        assert "op.getInput(1)" in java
        assert "instanceof Pointer" in java
        assert "getHigh()" in java
        assert "does not infer a class" in java
        assert "call target" in java

    print("nevergone static receiver vtable inference smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
