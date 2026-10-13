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


def receiver_row(source_id: str, class_name: str, call_index: int, *, fmt: int = 2,
                 pointer_depth: int = 1, flow: list[str] | None = None) -> dict[str, str]:
    flow = flow or []
    direct_pointee = class_name if pointer_depth == 1 else "-"
    return {
        "format": f"NEVERGONE.GhidraIndirectReceiverType/{fmt}",
        "source_id": source_id,
        "function_entry": source_id.split(":", 1)[1],
        "call_index": str(call_index),
        "call_address": f"00{call_index+1:06x}",
        "call_time": "0",
        "arg_count": "1",
        "receiver_present": "1",
        "receiver_storage_kind": "unique",
        "receiver_token": "def:CAST@0:0:4",
        "receiver_type_name": (class_name.split("::")[-1] + " *") if pointer_depth == 1 else "void **",
        "receiver_type_display": (class_name + " *") if pointer_depth == 1 else "void **",
        "receiver_pointer_depth": str(pointer_depth),
        "pointee_type_name": direct_pointee.split("::")[-1] if direct_pointee != "-" else "-",
        "pointee_type_display": direct_pointee,
        "flow_type_node_count": str(max(1, len(flow))),
        "flow_single_pointer_pointee_names_json": json.dumps([x.split("::")[-1] for x in flow]),
        "flow_single_pointer_pointee_displays_json": json.dumps(flow),
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

        sources = [
            ("libcocos2dcpp.so:0029ec10", ["0x24", "0x8"]),
            ("libcocos2dcpp.so:0029ed0c", ["0x24", "0x8"]),
            ("libcocos2dcpp.so:00300000", ["0x10"]),
            ("libcocos2dcpp.so:00310000", ["0x10"]),
            ("libcocos2dcpp.so:00320000", ["0x10"]),
            ("libcocos2dcpp.so:00330000", ["0x10"]),
            ("libcocos2dcpp.so:00340000", ["0x10"]),
            ("libcocos2dcpp.so:00350000", ["0x10"]),
        ]
        write_jsonl(analysis, [analysis_row(source_id, slots) for source_id, slots in sources])

        fields = [
            "format", "source_id", "function_entry", "call_index", "call_address", "call_time",
            "arg_count", "receiver_present", "receiver_storage_kind", "receiver_token",
            "receiver_type_name", "receiver_type_display", "receiver_pointer_depth",
            "pointee_type_name", "pointee_type_display", "flow_type_node_count",
            "flow_single_pointer_pointee_names_json", "flow_single_pointer_pointee_displays_json",
        ]
        receiver_rows: list[dict[str, str]] = []
        # /1 rows prove backward compatibility with the original direct receiver evidence.
        for source_id, class_name in [
            ("libcocos2dcpp.so:0029ec10", "ActionData"),
            ("libcocos2dcpp.so:0029ed0c", "EenemyActionDataSet"),
        ]:
            for i in range(2):
                row = receiver_row(source_id, class_name, i, fmt=1)
                row["flow_type_node_count"] = ""
                row["flow_single_pointer_pointee_names_json"] = ""
                row["flow_single_pointer_pointee_displays_json"] = ""
                receiver_rows.append(row)
        receiver_rows.extend([
            receiver_row("libcocos2dcpp.so:00300000", "cocos2d::CCNode", 0),
            receiver_row("libcocos2dcpp.so:00310000", "DupClass", 0),
            receiver_row("libcocos2dcpp.so:00320000", "void", 0),
            # Final receiver is void **, but a transparent CAST/COPY ancestor preserves RecoveredClass *.
            receiver_row("libcocos2dcpp.so:00330000", "void", 0, pointer_depth=2, flow=["RecoveredClass"]),
            # Conflicting transparent-flow classes must block inference.
            receiver_row("libcocos2dcpp.so:00340000", "void", 0, pointer_depth=2,
                         flow=["ActionData", "EenemyActionDataSet"]),
            # No usable single-pointer evidence remains conservatively blocked.
            receiver_row("libcocos2dcpp.so:00350000", "void", 0, pointer_depth=2, flow=[]),
        ])
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
            catalog_row("RecoveredClass", "libcocos2dcpp.so:00930008", "0x10", "libcocos2dcpp.so:00430000", "runtime_unobserved"),
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
        assert status_counts["resolved_unique_vtable"] == 2, status_counts
        assert status_counts["ambiguous_class_vtable"] == 1, status_counts
        assert status_counts["unresolved_receiver_class"] == 1, status_counts
        assert status_counts["ambiguous_receiver_class"] == 1, status_counts
        assert status_counts["receiver_not_single_pointer"] == 1, status_counts

        recovered = next(x for x in inferred if x["source_id"].endswith(":00330000"))
        assert recovered["class_name"] == "RecoveredClass", recovered
        assert recovered["class_evidence_source"] == "transparent_type_flow", recovered
        assert recovered["class_match_kind"].startswith("transparent_flow_"), recovered
        ambiguous_flow = next(x for x in inferred if x["source_id"].endswith(":00340000"))
        assert ambiguous_flow["candidate_receiver_class_names"] == ["ActionData", "EenemyActionDataSet"], ambiguous_flow

        query_rows = [json.loads(x) for x in queries.read_text().splitlines() if x.strip()]
        assert len(query_rows) == 6, query_rows
        ccnode = next(x for x in query_rows if x["source_id"].endswith(":00300000"))
        assert ccnode["receiver_vtable_id"] == "libcocos2dcpp.so:00900008"
        recovered_query = next(x for x in query_rows if x["source_id"].endswith(":00330000"))
        assert recovered_query["receiver_vtable_id"] == "libcocos2dcpp.so:00930008"
        assert recovered_query["receiver_class_evidence_source"] == "transparent_type_flow"

        subprocess.run([
            sys.executable,
            str(ROOT / "tools/ml/resolve_nevergone_receiver_vtable_slots.py"),
            str(catalog), str(queries), "--output", str(resolutions),
        ], check=True)
        resolved = [json.loads(x) for x in resolutions.read_text().splitlines() if x.strip()]
        resolution_counts = Counter(x["status"] for x in resolved)
        assert resolution_counts["exact_key_runtime_verified"] == 4, resolution_counts
        assert resolution_counts["exact_key_single_candidate"] == 2, resolution_counts

        java = (ROOT / "tools/ghidra/ExportIndirectCallReceiverTypes.java").read_text(encoding="utf-8")
        for token in (
            'FORMAT = "NEVERGONE.GhidraIndirectReceiverType/2"',
            "MAX_FLOW_DEPTH = 4", "MAX_FLOW_NODES = 16", "instanceof TypeDef",
            "getBaseDataType()", "PcodeOp.COPY", "PcodeOp.CAST", "PcodeOp.INDIRECT",
            "PcodeOp.MULTIEQUAL", "flow_single_pointer_pointee_displays_json",
            "op.getInput(1)", "does not infer a class", "call target",
        ):
            assert token in java, token
        assert "PcodeOp.PTRADD" not in java
        assert "PcodeOp.PTRSUB" not in java

    print("nevergone static receiver vtable inference smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
