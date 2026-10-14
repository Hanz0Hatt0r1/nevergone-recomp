#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
AUDIT = ROOT / "tools/ml/audit_nevergone_gamesceneui_mixed_receiver_target.py"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def write_tsv(path: Path, fields: list[str], rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def run_case(root: Path, diverge: bool) -> list[dict]:
    singleton = root / "singleton.jsonl"
    consumer = root / "consumer.jsonl"
    subtrees = root / "subtrees.tsv"
    catalog = root / "catalog.jsonl"
    details = root / ("details-diverge.jsonl" if diverge else "details-ok.jsonl")

    write_jsonl(singleton, [{
        "format": "NEVERGONE.SingletonConstructorStoreReturnAudit/1",
        "function_entries": ["003e95e0"], "function_names": ["GameSceneUI::sharedGameSceneUI()"],
        "expected_function": "sharedGameSceneUI", "expected_class": "GameSceneUI",
        "constructor_calls": 1, "constructor_object_roots": ["def:CALL@003e9604:5:4"],
        "constructor_stores": 1, "singleton_storage_key": "0x9755c4", "returns": 1,
        "candidate_receiver_class_names": ["GameSceneUI"],
        "candidate_receiver_vtable_ids": ["lib.so:vt_gamesceneui"],
        "status": "singleton_constructor_store_return_class", "reasons": [], "affects_routing": False,
    }])
    write_jsonl(consumer, [{
        "format": "NEVERGONE.GameSceneUIControlsReturnConsumerAudit/1",
        "accessor_chain_ok": True, "direct_caller_functions": 2, "caller_functions_decompiled": 2,
        "caller_functions_failed": 0, "accessor_call_sites": 2, "accessor_calls_with_output": 2,
        "direct_member_uses": 3, "unsupported_uses": 0,
        "target_counts": {"Controls::tick": 2, "Controls::onCCJoyStickUpdate": 1},
        "mapped_classes": ["Controls"], "mapped_vtables": ["lib.so:vt_controls"],
        "unmapped_targets": [], "mapped_rows": [],
        "status": "unique_catalog_member_consumer_class", "reasons": [], "affects_routing": False,
    }])

    fields = [
        "format", "row_type", "source_id", "source_call_index", "source_entry", "parameter_index",
        "dispatch_slot_offset", "expected_direct_caller_functions", "expected_matched_caller_calls",
        "direct_caller_functions", "caller_functions_decompiled", "caller_functions_failed",
        "callers_with_matched_calls", "matched_caller_calls", "caller_entry", "caller_name",
        "caller_call_site", "node_id", "parent_node_id", "input_index", "depth", "edge_kind",
        "node_token", "type_display", "pointer_depth", "pointee_type_display", "constant_value",
        "defining_opcode", "defining_address", "defining_time", "call_target_entry", "call_target_name",
        "truncation", "status",
    ]
    rows: list[dict] = []
    for call in (3, 4, 7):
        common = {
            "format": "NEVERGONE.GhidraCallerArgumentSubtree/1", "source_id": "libcocos2dcpp.so:002a7594",
            "source_call_index": str(call), "source_entry": "002a7594", "parameter_index": "0",
            "dispatch_slot_offset": "0x80", "expected_direct_caller_functions": "1",
            "expected_matched_caller_calls": "3", "direct_caller_functions": "-",
            "caller_functions_decompiled": "-", "caller_functions_failed": "-",
            "callers_with_matched_calls": "-", "matched_caller_calls": "-", "caller_entry": "003f12a8",
            "caller_name": "caller", "caller_call_site": "003f1872", "type_display": "int",
            "pointer_depth": "0", "pointee_type_display": "-", "truncation": "-", "status": "-",
        }
        rows.extend([
            {**common, "row_type": "node", "node_id": "1", "parent_node_id": "0", "input_index": "0", "depth": "1", "edge_kind": "value", "node_token": "def:CALL", "constant_value": "-", "defining_opcode": "CALL", "defining_address": "003f1866", "defining_time": "1", "call_target_entry": "003e95e0", "call_target_name": "sharedGameSceneUI"},
            {**common, "row_type": "node", "node_id": "2", "parent_node_id": "0", "input_index": "1", "depth": "1", "edge_kind": "base", "node_token": "def:CALL", "constant_value": "-", "defining_opcode": "CALL", "defining_address": "003f188a", "defining_time": "2", "call_target_entry": "003e95e0", "call_target_name": "sharedGameSceneUI"},
            {**common, "row_type": "node", "node_id": "3", "parent_node_id": "0", "input_index": "2", "depth": "1", "edge_kind": "offset", "node_token": "const:0x134:4", "constant_value": "0x134", "defining_opcode": "-", "defining_address": "-", "defining_time": "-", "call_target_entry": "-", "call_target_name": "-"},
            {**common, "row_type": "summary", "node_id": "-", "parent_node_id": "-", "input_index": "-", "depth": "-", "edge_kind": "-", "node_token": "-", "constant_value": "-", "defining_opcode": "-", "defining_address": "-", "defining_time": "-", "call_target_entry": "-", "call_target_name": "-", "status": "complete_caller_argument_subtrees"},
        ])
    write_tsv(subtrees, fields, rows)

    common_target = "lib.so:00515d2f"
    write_jsonl(catalog, [
        {
            "format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "Controls",
            "receiver_vtable_id": "lib.so:vt_controls", "dispatch_slot_offset": "0x80",
            "candidate_target_ids": [common_target], "runtime_validation_status": "runtime_unobserved",
        },
        {
            "format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "GameSceneUI",
            "receiver_vtable_id": "lib.so:vt_gamesceneui", "dispatch_slot_offset": "0x80",
            "candidate_target_ids": ["lib.so:0051ffff" if diverge else common_target],
            "runtime_validation_status": "runtime_unobserved",
        },
    ])

    result = subprocess.run([
        sys.executable, str(AUDIT), str(singleton), str(consumer), str(subtrees), str(catalog),
        "--details-output", str(details),
    ], check=True, capture_output=True, text=True)
    result.check_returncode()
    output = [json.loads(line) for line in details.read_text(encoding="utf-8").splitlines() if line.strip()]
    assert len(output) == 3, result.stdout
    return output


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        ok = run_case(root, False)
        assert all(row["status"] == "unique_target_across_proven_receiver_classes" for row in ok), ok
        assert {row["convergent_target_id"] for row in ok} == {"lib.so:00515d2f"}, ok
        bad = run_case(root, True)
        assert all(row["status"] == "mixed_receiver_target_not_proven" for row in bad), bad
        assert all("proven_receiver_classes_do_not_converge_one_target" in row["reasons"] for row in bad), bad
    print("Nevergone GameSceneUI mixed receiver target smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
