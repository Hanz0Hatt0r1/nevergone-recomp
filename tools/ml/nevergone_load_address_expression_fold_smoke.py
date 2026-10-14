#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FOLD = ROOT / "tools/ml/audit_nevergone_load_address_expression_fold.py"
OWNER = ROOT / "tools/ml/audit_nevergone_load_effective_address_owners.py"
SLICE_FMT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
CLASS_FMT = "NEVERGONE.GhidraAddressClassification/1"
REF_FMT = "NEVERGONE.GhidraAddressReferenceOwners/1"

SLICE_FIELDS = [
    "format","row_type","function_entry","call_index","call_address","call_time","recovery_status","slot_offset",
    "structural_receiver_token","node_id","parent_node_id","input_index","depth","node_token","type_display",
    "pointer_depth","pointee_type_display","constant_value","defining_opcode","defining_address","defining_time",
    "call_target_entry","call_target_name","call_target_return_type","call_target_is_thunk","call_target_thunk_target",
    "truncation","node_count","truncated_by_budget","depth_truncation_count","cycle_count","max_nodes","max_depth",
]
CLASS_FIELDS = [
    "format","address","block","block_start","block_end","read","write","execute","primary_symbol",
    "function_at_entry","function_at_name","function_at_body","function_at_is_thunk","function_at_thunk_target",
    "function_containing_entry","function_containing_name","function_containing_body","instruction_mnemonic","data_type","raw_u32",
]
REF_FIELDS = [
    "format","target_address","target_block","target_symbol","reference_from","reference_type","reference_source",
    "operand_index","from_symbol","from_instruction","containing_function_entry","containing_function_name","nearby_class_evidence",
]


def write_tsv(path: Path, fields: list[str], rows: list[dict[str, object]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        for row in rows:
            writer.writerow({field: row.get(field, "-") for field in fields})


def node(node_id: int, parent: int, input_index: int, depth: int, token: str, opcode: str, const: str = "-") -> dict[str, object]:
    return {
        "format": SLICE_FMT,
        "row_type": "node",
        "function_entry": "00100000",
        "call_index": "0",
        "call_address": "00100010",
        "call_time": "1",
        "recovery_status": "canonical_receiver",
        "slot_offset": "0x8",
        "structural_receiver_token": "def:LOAD@00100008:1:4",
        "node_id": str(node_id),
        "parent_node_id": str(parent),
        "input_index": str(input_index),
        "depth": str(depth),
        "node_token": token,
        "type_display": "int",
        "pointer_depth": "0",
        "pointee_type_display": "-",
        "constant_value": const,
        "defining_opcode": opcode,
        "defining_address": "00100008",
        "defining_time": "1",
        "max_nodes": "96",
        "max_depth": "16",
    }


def class_row(address: str, block: str, read: str, write: str, execute: str, raw: str, symbol: str = "-", instruction: str = "-", data_type: str = "-") -> dict[str, object]:
    return {
        "format": CLASS_FMT,
        "address": address,
        "block": block,
        "block_start": "00001000",
        "block_end": "0000ffff",
        "read": read,
        "write": write,
        "execute": execute,
        "primary_symbol": symbol,
        "instruction_mnemonic": instruction,
        "data_type": data_type,
        "raw_u32": raw,
    }


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        d = Path(tmp)
        slices = d / "slices"
        slices.mkdir()
        failures = d / "failures.jsonl"
        memory = d / "memory.tsv"
        fold_details = d / "fold.jsonl"
        effective = d / "effective.tsv"
        values = d / "values.tsv"
        refs = d / "refs.tsv"
        owner_details = d / "owners.jsonl"

        failures.write_text(json.dumps({
            "source_id": "test.so:00100000",
            "call_index": 0,
            "call_address": "00100010",
            "dispatch_slot_offset": "0x8",
            "status": "receiver_not_single_pointer",
        }) + "\n", encoding="utf-8")

        rows = [
            node(0, -1, -1, 0, "def:LOAD@00100008:1:4", "LOAD"),
            node(1, 0, 1, 1, "def:INT_ADD@00100006:2:4", "INT_ADD"),
            node(2, 1, 0, 2, "def:LOAD@00100004:3:4", "LOAD"),
            node(3, 1, 1, 2, "const:0x10:4", "-", "0x10"),
            node(4, 2, 1, 3, "address:00100020:4", "-"),
        ]
        write_tsv(slices / "test.so_00100000.tsv", SLICE_FIELDS, rows)
        write_tsv(memory, CLASS_FIELDS, [class_row("00100020", ".text", "1", "0", "1", "0x00009000", data_type="undefined4")])

        fold_out = subprocess.check_output([
            "python3", str(FOLD), str(failures), str(slices), str(memory), "--details-output", str(fold_details)
        ], text=True)
        assert "effective= 0x00009010" in fold_out, fold_out
        parsed = [json.loads(line) for line in fold_details.read_text().splitlines() if line]
        assert len(parsed) == 1
        assert parsed[0]["effective_address"] == 0x9010
        assert parsed[0]["status"] == "effective_address_folded"
        assert parsed[0]["resolved_memory_reads"] == [0x100020]

        write_tsv(effective, CLASS_FIELDS, [class_row("00009010", ".data", "1", "1", "0", "0x0000a000", "PTR_current", data_type="pointer")])
        write_tsv(values, CLASS_FIELDS, [class_row("0000a000", ".data.rel.ro", "1", "0", "0", "0x00000000", "vtable_for_Test_Thing", data_type="pointer")])
        write_tsv(refs, REF_FIELDS, [{
            "format": REF_FMT,
            "target_address": "00009010",
            "target_block": ".data",
            "target_symbol": "PTR_current",
            "reference_from": "00100004",
            "reference_type": "READ",
            "reference_source": "ANALYSIS",
            "operand_index": "1",
            "from_symbol": "-",
            "from_instruction": "ldr",
            "containing_function_entry": "00100000",
            "containing_function_name": "TestManager::end",
            "nearby_class_evidence": "TestManager::end",
        }])

        owner_out = subprocess.check_output([
            "python3", str(OWNER), str(fold_details), str(effective), str(values), str(refs), "--details-output", str(owner_details)
        ], text=True)
        assert "writable_effective_cell_with_refs" in owner_out, owner_out
        owners = [json.loads(line) for line in owner_details.read_text().splitlines() if line]
        assert owners[0]["effective_address"] == "00009010"
        assert owners[0]["raw_value_address"] == "0000a000"
        assert owners[0]["owner_functions"] == ["TestManager::end"]

    print("LOAD address expression fold smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
