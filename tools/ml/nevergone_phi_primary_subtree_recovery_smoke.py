#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FINALIZER = ROOT / "tools/ml/finalize_nevergone_phi_primary_subtree_recovery.py"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row) + "\n")


def write_tsv(path: Path, fields: list[str], rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t", extrasaction="ignore")
        writer.writeheader()
        writer.writerows(rows)


def inference(source_id: str, address: str, status: str, *, cls=None, vtable=None) -> dict:
    return {
        "format": "NEVERGONE.StaticReceiverVtableInference/1",
        "source_id": source_id,
        "source_entry": source_id.split(":", 1)[1],
        "call_index": 0,
        "call_address": address,
        "dispatch_slot_offset": "0x14",
        "receiver_type_name": None,
        "receiver_type_display": None,
        "class_name": cls,
        "class_match_kind": "exact_catalog_class_name" if cls else None,
        "class_evidence_source": "direct_receiver_type" if cls else None,
        "candidate_receiver_class_names": [cls] if cls else [],
        "candidate_receiver_vtable_ids": [vtable] if vtable else [],
        "receiver_vtable_id": vtable,
        "runtime_validation_status": "runtime_unobserved",
        "status": status,
        "affects_routing": False,
    }


def detail(source_id: str, owner: str) -> dict:
    return {
        "format": "NEVERGONE.PhiDefaultInstanceRecovery/1",
        "source_id": source_id,
        "call_index": 0,
        "eligible": True,
        "accepted": False,
        "primary_branch_classes": [],
        "secondary_got_address": "0x00009000",
        "default_instance_address": "0x00009100",
        "owner_classes": [owner],
        "owner_direct_method_xrefs": 4,
        "reasons": ["primary_branch_class_not_unique"],
        "affects_routing": False,
    }


def slice_rows(entry: str, address: str, typed_class: str) -> list[dict]:
    base = {
        "format": "NEVERGONE.GhidraStructuralReceiverSlice/1",
        "function_entry": entry,
        "call_index": "0",
        "call_address": address,
        "slot_offset": "0x14",
    }
    return [
        {**base, "row_type": "summary", "recovery_status": "canonical_receiver"},
        {**base, "row_type": "node", "node_id": "0", "parent_node_id": "-1", "input_index": "-1", "defining_opcode": "MULTIEQUAL", "type_display": "int *", "pointer_depth": "1", "pointee_type_display": "int"},
        {**base, "row_type": "node", "node_id": "10", "parent_node_id": "0", "input_index": "0", "defining_opcode": "COPY", "type_display": "int *", "pointer_depth": "1", "pointee_type_display": "int"},
        {**base, "row_type": "node", "node_id": "11", "parent_node_id": "10", "input_index": "0", "defining_opcode": "COPY", "type_display": f"{typed_class} *", "pointer_depth": "1", "pointee_type_display": typed_class},
        {**base, "row_type": "node", "node_id": "2", "parent_node_id": "0", "input_index": "1", "defining_opcode": "LOAD", "type_display": "int *", "pointer_depth": "1", "pointee_type_display": "int"},
    ]


def main() -> int:
    with tempfile.TemporaryDirectory() as tmpdir:
        root = Path(tmpdir)
        slices = root / "slices"
        slices.mkdir()
        inf = root / "v7.jsonl"
        queries = root / "q7.jsonl"
        details = root / "details7.jsonl"
        catalog = root / "catalog.jsonl"
        out = root / "v8.jsonl"
        outq = root / "q8.jsonl"
        outdetails = root / "details8.jsonl"

        write_jsonl(inf, [
            inference("lib:test.so:00100000", "00100004", "receiver_not_single_pointer"),
            inference("lib:test.so:00100010", "00100014", "receiver_not_single_pointer"),
            inference("lib:test.so:00100020", "00100024", "resolved_unique_vtable", cls="Test::Existing", vtable="vt_existing"),
        ])
        write_jsonl(queries, [{
            "format": "NEVERGONE.StaticReceiverVtableQuery/1",
            "source_id": "lib:test.so:00100020",
            "call_index": 0,
            "call_address": "00100024",
            "receiver_vtable_id": "vt_existing",
            "dispatch_slot_offset": "0x14",
            "receiver_class_name": "Test::Existing",
            "receiver_class_match_kind": "exact_catalog_class_name",
            "receiver_class_evidence_source": "direct_receiver_type",
            "receiver_type_name": None,
            "receiver_type_display": None,
            "runtime_validation_status": "runtime_unobserved",
            "evidence_ref": "lib:test.so:00100020#call=0",
            "affects_routing": False,
        }])
        write_jsonl(details, [
            detail("lib:test.so:00100000", "Test::A"),
            detail("lib:test.so:00100010", "Test::B"),
        ])
        write_jsonl(catalog, [
            {"format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "Test::A", "receiver_vtable_id": "vt_a"},
            {"format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "Test::B", "receiver_vtable_id": "vt_b"},
            {"format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "Test::Existing", "receiver_vtable_id": "vt_existing"},
        ])

        fields = [
            "format", "row_type", "function_entry", "call_index", "call_address",
            "recovery_status", "slot_offset", "node_id", "parent_node_id", "input_index",
            "defining_opcode", "type_display", "pointer_depth", "pointee_type_display",
        ]
        write_tsv(slices / "a.tsv", fields, slice_rows("00100000", "00100004", "Test::A"))
        write_tsv(slices / "b.tsv", fields, slice_rows("00100010", "00100014", "Test::A"))

        cmd = [
            sys.executable, str(FINALIZER),
            str(inf), str(queries), str(details), str(slices), str(catalog),
            "--output", str(out), "--queries-output", str(outq), "--details-output", str(outdetails),
        ]
        subprocess.run(cmd, check=True, capture_output=True, text=True)

        rows = [json.loads(line) for line in out.read_text().splitlines() if line.strip()]
        by_id = {row["source_id"]: row for row in rows}
        accepted = by_id["lib:test.so:00100000"]
        blocked = by_id["lib:test.so:00100010"]
        existing = by_id["lib:test.so:00100020"]

        assert accepted["status"] == "resolved_unique_vtable"
        assert accepted["class_name"] == "Test::A"
        assert accepted["receiver_vtable_id"] == "vt_a"
        assert accepted["class_evidence_source"] == "structural_phi_primary_subtree_default_instance_owner"
        assert accepted["phi_primary_subtree_recovery"]["primary_subtree_classes"] == ["Test::A"]
        assert any(node["node_id"] == 11 for node in accepted["phi_primary_subtree_recovery"]["primary_subtree_typed_nodes"])

        assert blocked["status"] == "receiver_not_single_pointer"
        assert blocked["phi_primary_subtree_recovery"]["reasons"] == ["primary_subtree_owner_mismatch"]
        assert existing["status"] == "resolved_unique_vtable"

        qrows = [json.loads(line) for line in outq.read_text().splitlines() if line.strip()]
        assert {(row["source_id"], row["call_index"]) for row in qrows} == {
            ("lib:test.so:00100000", 0),
            ("lib:test.so:00100020", 0),
        }
        print("Nevergone PHI primary-subtree recovery smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
