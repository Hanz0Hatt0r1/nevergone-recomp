#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def analysis_row(source_id: str) -> dict:
    return {
        "id": source_id,
        "indirect_call_slice_evidence": {
            "present": True,
            "calls": [{
                "call_index": 0,
                "call_address": source_id.split(":", 1)[1],
                "primary_slot_offset_candidate": "0x4",
            }],
        },
    }


def receiver_row(source_id: str) -> dict[str, str]:
    return {
        "format": "NEVERGONE.GhidraIndirectReceiverType/2",
        "source_id": source_id,
        "function_entry": source_id.split(":", 1)[1],
        "call_index": "0",
        "call_address": source_id.split(":", 1)[1],
        "call_time": "0",
        "arg_count": "0",
        "receiver_present": "0",
        "receiver_storage_kind": "missing",
        "receiver_token": "-",
        "receiver_type_name": "-",
        "receiver_type_display": "-",
        "receiver_pointer_depth": "0",
        "pointee_type_name": "-",
        "pointee_type_display": "-",
        "flow_type_node_count": "0",
        "flow_single_pointer_pointee_names_json": "[]",
        "flow_single_pointer_pointee_displays_json": "[]",
    }


def parameter_row(source_id: str) -> dict[str, str]:
    return {
        "format": "NEVERGONE.GhidraReceiverParameterProvenance/1",
        "source_id": source_id,
        "function_entry": source_id.split(":", 1)[1],
        "call_index": "0",
        "call_address": source_id.split(":", 1)[1],
        "status": "not_load",
        "parameter_index": "-",
        "direct_caller_count": "0",
        "matched_caller_calls": "0",
        "caller_single_pointer_pointee_names_json": "[]",
        "caller_single_pointer_pointee_displays_json": "[]",
    }


def structural_row(source_id: str, classes: list[str]) -> dict[str, str]:
    return {
        "format": "NEVERGONE.GhidraStructuralReceiverParameterProvenance/1",
        "source_id": source_id,
        "function_entry": source_id.split(":", 1)[1],
        "call_index": "0",
        "call_address": source_id.split(":", 1)[1],
        "direct_receiver_present": "0",
        "structural_receiver_present": "1",
        "structural_receiver_token": "def:LOAD@1:0:4",
        "structural_receiver_type_display": "int *",
        "structural_receiver_pointer_depth": "1",
        "structural_receiver_pointee_type_display": "int",
        "structural_receiver_is_load": "1",
        "load_address_token": "register:r0:4",
        "root_token": "register:r0:4",
        "root_type_display": classes[0] + " *" if classes else "int",
        "root_trace_patterns_json": "[]",
        "root_trace_nodes": "1",
        "parameter_index": "0",
        "parameter_type_display": classes[0] + " *" if classes else "int",
        "direct_caller_count": "1",
        "matched_caller_calls": "1",
        "caller_entries_json": "[\"00100000\"]",
        "caller_call_sites_json": "[\"00100010\"]",
        "caller_single_pointer_pointee_names_json": json.dumps([x.split("::")[-1] for x in classes]),
        "caller_single_pointer_pointee_displays_json": json.dumps(classes),
        "status": "caller_type_candidate",
    }


def catalog_row(class_name: str, vtable: str) -> dict:
    return {
        "format": "NEVERGONE.StaticVtableSlotCatalog/3",
        "class_name": class_name,
        "receiver_vtable_id": vtable,
        "dispatch_slot_offset": "0x4",
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        tmp = Path(td)
        unique_id = "libcocos2dcpp.so:00400000"
        ambiguous_id = "libcocos2dcpp.so:00410000"
        analysis = tmp / "analysis.jsonl"
        receivers = tmp / "receivers.tsv"
        parameter = tmp / "parameter.tsv"
        structural = tmp / "structural.tsv"
        catalog = tmp / "catalog.jsonl"
        output = tmp / "output.jsonl"
        queries = tmp / "queries.jsonl"

        write_jsonl(analysis, [analysis_row(unique_id), analysis_row(ambiguous_id)])

        receiver_fields = list(receiver_row(unique_id).keys())
        with receivers.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=receiver_fields, delimiter="\t")
            writer.writeheader()
            writer.writerows([receiver_row(unique_id), receiver_row(ambiguous_id)])

        parameter_fields = list(parameter_row(unique_id).keys())
        with parameter.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=parameter_fields, delimiter="\t")
            writer.writeheader()
            writer.writerows([parameter_row(unique_id), parameter_row(ambiguous_id)])

        unique = structural_row(unique_id, ["PVEDATA::equipToolsListMsg"])
        ambiguous = structural_row(ambiguous_id, ["ClassA", "ClassB"])
        structural_fields = list(unique.keys())
        with structural.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=structural_fields, delimiter="\t")
            writer.writeheader()
            writer.writerows([unique, ambiguous])

        write_jsonl(catalog, [
            catalog_row("PVEDATA::equipToolsListMsg", "libcocos2dcpp.so:00990008"),
            catalog_row("ClassA", "libcocos2dcpp.so:009a0008"),
            catalog_row("ClassB", "libcocos2dcpp.so:009b0008"),
        ])

        subprocess.run([
            sys.executable,
            str(ROOT / "tools/ml/infer_nevergone_static_receiver_vtables.py"),
            str(analysis), str(receivers), str(catalog),
            "--parameter-provenance", str(parameter),
            "--structural-parameter-provenance", str(structural),
            "--output", str(output),
            "--queries-output", str(queries),
        ], check=True)

        rows = [json.loads(line) for line in output.read_text().splitlines() if line.strip()]
        assert len(rows) == 2, rows
        recovered = next(row for row in rows if row["source_id"] == unique_id)
        assert recovered["status"] == "resolved_unique_vtable", recovered
        assert recovered["class_name"] == "PVEDATA::equipToolsListMsg", recovered
        assert recovered["class_evidence_source"] == "structural_parameter_caller_type", recovered
        assert recovered["class_match_kind"].startswith("structural_parameter_caller_"), recovered
        assert recovered["receiver_vtable_id"] == "libcocos2dcpp.so:00990008", recovered

        blocked = next(row for row in rows if row["source_id"] == ambiguous_id)
        assert blocked["status"] == "ambiguous_receiver_class", blocked
        assert blocked["candidate_receiver_class_names"] == ["ClassA", "ClassB"], blocked

        query_rows = [json.loads(line) for line in queries.read_text().splitlines() if line.strip()]
        assert len(query_rows) == 1, query_rows
        assert query_rows[0]["receiver_class_evidence_source"] == "structural_parameter_caller_type"

    print("nevergone structural receiver provenance smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
