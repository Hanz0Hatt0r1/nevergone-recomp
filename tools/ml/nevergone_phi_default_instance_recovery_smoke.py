#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FINALIZER = ROOT / "tools/ml/finalize_nevergone_phi_default_instance_recovery.py"


def write_jsonl(path: Path, rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8") as f:
        for row in rows:
            f.write(json.dumps(row) + "\n")


def write_tsv(path: Path, fieldnames: list[str], rows: list[dict]) -> None:
    with path.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fieldnames, delimiter="\t", extrasaction="ignore")
        w.writeheader()
        w.writerows(rows)


def inference(source_id: str, call_address: str, status: str, *, cls=None, vt=None, source=None) -> dict:
    return {
        "format": "NEVERGONE.StaticReceiverVtableInference/1",
        "source_id": source_id,
        "source_entry": source_id.split(":", 1)[1],
        "call_index": 0,
        "call_address": call_address,
        "dispatch_slot_offset": "0x14",
        "receiver_type_name": None,
        "receiver_type_display": None,
        "class_name": cls,
        "class_match_kind": "exact_catalog_class_name" if cls else None,
        "class_evidence_source": source,
        "candidate_receiver_class_names": [cls] if cls else [],
        "candidate_receiver_vtable_ids": [vt] if vt else [],
        "receiver_vtable_id": vt,
        "runtime_validation_status": "runtime_unobserved",
        "status": status,
        "affects_routing": False,
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as td:
        d = Path(td)
        slice_dir = d / "slices"
        slice_dir.mkdir()
        inf = d / "inference.jsonl"
        q = d / "queries.jsonl"
        phi = d / "phi.jsonl"
        got = d / "got.tsv"
        targets = d / "targets.tsv"
        owners = d / "owners.tsv"
        target_prov = d / "target-prov.tsv"
        catalog = d / "catalog.jsonl"
        out = d / "out.jsonl"
        outq = d / "outq.jsonl"
        details = d / "details.jsonl"

        write_jsonl(inf, [
            inference("lib:test.so:00100000", "00100004", "receiver_not_single_pointer"),
            inference("lib:test.so:00100010", "00100014", "receiver_not_single_pointer"),
            inference("lib:test.so:00100020", "00100024", "resolved_unique_vtable", cls="Test::Existing", vt="vt_existing", source="direct_receiver_type"),
        ])
        write_jsonl(q, [{
            "format": "NEVERGONE.StaticReceiverVtableQuery/1",
            "source_id": "lib:test.so:00100020", "call_index": 0, "call_address": "00100024",
            "receiver_vtable_id": "vt_existing", "dispatch_slot_offset": "0x14",
            "receiver_class_name": "Test::Existing", "receiver_class_match_kind": "exact_catalog_class_name",
            "receiver_class_evidence_source": "direct_receiver_type", "receiver_type_name": None,
            "receiver_type_display": None, "runtime_validation_status": "runtime_unobserved",
            "evidence_ref": "lib:test.so:00100020#call=0", "affects_routing": False,
        }])
        write_jsonl(catalog, [
            {"format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "Test::A", "receiver_vtable_id": "vt_a"},
            {"format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "Test::B", "receiver_vtable_id": "vt_b"},
            {"format": "NEVERGONE.StaticVtableSlotCatalog/3", "class_name": "Test::Existing", "receiver_vtable_id": "vt_existing"},
        ])

        slice_fields = ["format","row_type","function_entry","call_index","call_address","recovery_status","slot_offset","node_id","parent_node_id","input_index","defining_opcode","type_display","pointer_depth","pointee_type_display"]
        for entry, addr in [("00100000", "00100004"), ("00100010", "00100014")]:
            write_tsv(slice_dir / f"slice-{entry}.tsv", slice_fields, [
                {"format":"NEVERGONE.GhidraStructuralReceiverSlice/1","row_type":"summary","function_entry":entry,"call_index":"0","call_address":addr,"recovery_status":"canonical_receiver","slot_offset":"0x14"},
                {"format":"NEVERGONE.GhidraStructuralReceiverSlice/1","row_type":"node","function_entry":entry,"call_index":"0","call_address":addr,"slot_offset":"0x14","node_id":"0","parent_node_id":"-1","input_index":"-1","defining_opcode":"MULTIEQUAL","type_display":"int *","pointer_depth":"1","pointee_type_display":"int"},
                {"format":"NEVERGONE.GhidraStructuralReceiverSlice/1","row_type":"node","function_entry":entry,"call_index":"0","call_address":addr,"slot_offset":"0x14","node_id":"1","parent_node_id":"0","input_index":"0","defining_opcode":"COPY","type_display":"Test::A *","pointer_depth":"1","pointee_type_display":"Test::A"},
                {"format":"NEVERGONE.GhidraStructuralReceiverSlice/1","row_type":"node","function_entry":entry,"call_index":"0","call_address":addr,"slot_offset":"0x14","node_id":"2","parent_node_id":"0","input_index":"1","defining_opcode":"LOAD","type_display":"int *","pointer_depth":"1","pointee_type_display":"int"},
            ])

        write_jsonl(phi, [
            {"format":"NEVERGONE.PhiSecondaryLoadTreeAudit/2","function_entry":"00100000","call_index":0,"call_address":"00100004","slot_offset":"0x14","secondary_node_id":2,"secondary_opcode":"LOAD","folded_value":None,"literal_pool_substitutions":[0x100],"unresolved_load_addresses":[0x9000],"affects_routing":False},
            {"format":"NEVERGONE.PhiSecondaryLoadTreeAudit/2","function_entry":"00100010","call_index":0,"call_address":"00100014","slot_offset":"0x14","secondary_node_id":2,"secondary_opcode":"LOAD","folded_value":None,"literal_pool_substitutions":[0x104],"unresolved_load_addresses":[0x9004],"affects_routing":False},
        ])

        addr_fields = ["format","address","block","primary_symbol","raw_u32"]
        write_tsv(got, addr_fields, [
            {"format":"NEVERGONE.GhidraAddressClassification/1","address":"00009000","block":".got","primary_symbol":"PTR_default_instance__00009000","raw_u32":"0x9100"},
            {"format":"NEVERGONE.GhidraAddressClassification/1","address":"00009004","block":".got","primary_symbol":"PTR_default_instance__00009004","raw_u32":"0x9104"},
        ])
        write_tsv(targets, addr_fields, [
            {"format":"NEVERGONE.GhidraAddressClassification/1","address":"00009100","block":".bss","primary_symbol":"default_instance_","raw_u32":"-"},
            {"format":"NEVERGONE.GhidraAddressClassification/1","address":"00009104","block":".bss","primary_symbol":"default_instance_","raw_u32":"-"},
        ])

        owner_fields = ["format","target_address","target_block","target_symbol","reference_from","reference_type","reference_source","operand_index","from_symbol","from_instruction","containing_function_entry","containing_function_name","nearby_class_evidence"]
        write_tsv(owners, owner_fields, [
            {"format":"NEVERGONE.GhidraAddressReferenceOwners/1","target_address":"00009100","target_block":".bss","target_symbol":"Test::A::default_instance_","reference_from":"00110000","reference_type":"READ","reference_source":"ANALYSIS","operand_index":"1","from_symbol":"-","from_instruction":"ldr","containing_function_entry":"00110000","containing_function_name":"Test::A::IsInitialized","nearby_class_evidence":"Test::A::default_instance_"},
            {"format":"NEVERGONE.GhidraAddressReferenceOwners/1","target_address":"00009104","target_block":".bss","target_symbol":"Test::B::default_instance_","reference_from":"00110010","reference_type":"READ","reference_source":"ANALYSIS","operand_index":"1","from_symbol":"-","from_instruction":"ldr","containing_function_entry":"00110010","containing_function_name":"Test::B::IsInitialized","nearby_class_evidence":"Test::B::default_instance_"},
        ])

        prov_fields = ["format","source_id","call_index","structural_status","target_slot_offset"]
        write_tsv(target_prov, prov_fields, [
            {"format":"NEVERGONE.GhidraTargetReceiverProvenance/1","source_id":"lib:test.so:00100000","call_index":"0","structural_status":"canonical_receiver","target_slot_offset":"0x14"},
            {"format":"NEVERGONE.GhidraTargetReceiverProvenance/1","source_id":"lib:test.so:00100010","call_index":"0","structural_status":"canonical_receiver","target_slot_offset":"0x14"},
        ])

        cmd = [sys.executable, str(FINALIZER), str(inf), str(q), str(slice_dir), str(phi), str(got), str(targets), str(owners), str(target_prov), str(catalog), "--output", str(out), "--queries-output", str(outq), "--details-output", str(details)]
        subprocess.run(cmd, check=True, capture_output=True, text=True)

        rows = [json.loads(x) for x in out.read_text().splitlines() if x.strip()]
        by_id = {r["source_id"]: r for r in rows}
        accepted = by_id["lib:test.so:00100000"]
        blocked = by_id["lib:test.so:00100010"]
        existing = by_id["lib:test.so:00100020"]
        assert accepted["status"] == "resolved_unique_vtable"
        assert accepted["class_name"] == "Test::A"
        assert accepted["receiver_vtable_id"] == "vt_a"
        assert accepted["class_evidence_source"] == "structural_phi_default_instance_owner"
        assert blocked["status"] == "receiver_not_single_pointer"
        assert "phi_branch_class_mismatch" in blocked["phi_default_instance_recovery"]["reasons"]
        assert existing["status"] == "resolved_unique_vtable"

        out_queries = [json.loads(x) for x in outq.read_text().splitlines() if x.strip()]
        assert {(r["source_id"], r["call_index"]) for r in out_queries} == {
            ("lib:test.so:00100000", 0), ("lib:test.so:00100020", 0)
        }

        detail_rows = [json.loads(x) for x in details.read_text().splitlines() if x.strip()]
        assert len(detail_rows) == 2
        assert sum(1 for r in detail_rows if r["accepted"]) == 1
        print("Nevergone PHI default-instance recovery smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
