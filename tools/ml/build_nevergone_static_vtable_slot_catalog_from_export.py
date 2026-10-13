#!/usr/bin/env python3
"""Build conservative class/vtable@slot candidates from ExportVtableSlots.java output.

Unlike the legacy xref-based catalog, this tool consumes slot pointers read directly
from analyzed Ghidra program memory. Every exported row already passed an exact
FunctionManager.getFunctionAt() check in Ghidra. Runtime evidence is validation-only:
static candidates never change routing or cross vtable/class boundaries.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import json
from collections import defaultdict
from pathlib import Path
from typing import Any

from analyze_nevergone_vtable_static_provenance import (
    ITANIUM_ARM32_ADDRESS_POINT_DELTA,
    addr,
    symbol_class_name,
)

FORMAT = "NEVERGONE.StaticVtableSlotCatalog/2"
MODULE = "libcocos2dcpp.so"
POINTER_SIZE = 4


def open_tsv(path: Path):
    return gzip.open(path, "rt", encoding="utf-8", errors="replace", newline="")


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            rows.append(row)
    return rows


def function_id(entry: int) -> str:
    return f"{MODULE}:{entry:08x}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("vtable_slots_tsv_gz", type=Path)
    parser.add_argument("--runtime-catalog", type=Path,
                        help="optional NEVERGONE.ClassDispatchCatalog/2 JSONL for validation only")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    runtime_by_key: dict[tuple[str, str], set[str]] = {}
    if args.runtime_catalog is not None:
        for row in read_jsonl(args.runtime_catalog):
            vtable_id = row.get("receiver_vtable_id")
            slot_offset = row.get("dispatch_slot_offset")
            targets = row.get("target_ids")
            if not isinstance(vtable_id, str) or not isinstance(slot_offset, str):
                continue
            if not isinstance(targets, list) or not all(isinstance(x, str) for x in targets):
                continue
            runtime_by_key[(vtable_id, slot_offset)] = set(targets)

    grouped: dict[tuple[int, str, int], list[dict[str, Any]]] = defaultdict(list)
    decoded_vtables: set[tuple[int, str]] = set()
    input_rows = 0
    rejected_layout_rows = 0

    with open_tsv(args.vtable_slots_tsv_gz) as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        required = {
            "vtable_symbol_start", "vtable_symbol_name", "address_point",
            "slot_offset", "slot_address", "raw_value", "target_thumb",
            "target_entry", "target_name", "target_signature", "resolution_mode",
            "next_vtable_symbol_start",
        }
        missing = required - set(reader.fieldnames or [])
        if missing:
            raise ValueError(f"missing vtable slot columns: {sorted(missing)}")

        for line_no, row in enumerate(reader, 2):
            input_rows += 1
            start = addr(row.get("vtable_symbol_start"))
            address_point = addr(row.get("address_point"))
            slot_offset = addr(row.get("slot_offset"))
            slot_address = addr(row.get("slot_address"))
            raw_value = addr(row.get("raw_value"))
            target_entry = addr(row.get("target_entry"))
            symbol_name = str(row.get("vtable_symbol_name") or "")
            class_name, evidence_kind = symbol_class_name(symbol_name)

            if None in (start, address_point, slot_offset, slot_address, raw_value, target_entry):
                raise ValueError(f"{args.vtable_slots_tsv_gz}:{line_no}: invalid address field")
            assert start is not None and address_point is not None
            assert slot_offset is not None and slot_address is not None
            assert raw_value is not None and target_entry is not None

            if not class_name or evidence_kind is None:
                rejected_layout_rows += 1
                continue
            if address_point - start != ITANIUM_ARM32_ADDRESS_POINT_DELTA:
                rejected_layout_rows += 1
                continue
            if slot_offset < 0 or slot_offset % POINTER_SIZE:
                rejected_layout_rows += 1
                continue
            if slot_address != address_point + slot_offset:
                rejected_layout_rows += 1
                continue

            next_start = addr(row.get("next_vtable_symbol_start"))
            if next_start is not None and slot_address >= next_start:
                raise ValueError(
                    f"{args.vtable_slots_tsv_gz}:{line_no}: slot crossed next vtable boundary")

            decoded_vtables.add((address_point, class_name))
            grouped[(address_point, class_name, slot_offset)].append({
                "vtable_symbol_start": start,
                "vtable_symbol_name": symbol_name,
                "class_evidence_kind": evidence_kind,
                "address_point": address_point,
                "slot_offset": slot_offset,
                "slot_address": slot_address,
                "raw_value": raw_value,
                "target_thumb": str(row.get("target_thumb") or "").lower() == "true",
                "target_entry": target_entry,
                "target_name": row.get("target_name"),
                "target_signature": row.get("target_signature"),
                "resolution_mode": row.get("resolution_mode"),
                "next_vtable_symbol_start": next_start,
            })

    out: list[dict[str, Any]] = []
    for (address_point, class_name, slot_offset), rows in sorted(grouped.items()):
        vtable_id = f"{MODULE}:{address_point:08x}"
        slot_hex = f"0x{slot_offset:x}"
        target_entries = sorted({int(row["target_entry"]) for row in rows})
        target_ids = [function_id(entry) for entry in target_entries]
        target_functions = []
        for entry in target_entries:
            samples = [row for row in rows if int(row["target_entry"]) == entry]
            sample = samples[0]
            target_functions.append({
                "target_id": function_id(entry),
                "entry": f"{entry:08x}",
                "name": sample.get("target_name"),
                "signature": sample.get("target_signature"),
                "thumb_observed_in_pointer": any(bool(x.get("target_thumb")) for x in samples),
                "raw_values": sorted({f"0x{int(x['raw_value']):x}" for x in samples}),
                "resolution_modes": sorted({str(x.get("resolution_mode")) for x in samples}),
            })

        runtime_targets = runtime_by_key.get((vtable_id, slot_hex))
        if runtime_targets is None:
            runtime_validation = "runtime_unobserved"
        elif set(target_ids) == runtime_targets:
            runtime_validation = "runtime_match"
        else:
            runtime_validation = "runtime_mismatch"

        next_starts = sorted({
            int(row["next_vtable_symbol_start"])
            for row in rows
            if row.get("next_vtable_symbol_start") is not None
        })
        starts = sorted({int(row["vtable_symbol_start"]) for row in rows})
        symbol_names = sorted({str(row["vtable_symbol_name"]) for row in rows})

        out.append({
            "format": FORMAT,
            "module": MODULE,
            "class_name": class_name,
            "class_evidence_kind": rows[0]["class_evidence_kind"],
            "vtable_symbol_names": symbol_names,
            "vtable_symbol_starts": [f"0x{x:x}" for x in starts],
            "receiver_vtable_address_point": f"0x{address_point:x}",
            "receiver_vtable_id": vtable_id,
            "address_point_delta": ITANIUM_ARM32_ADDRESS_POINT_DELTA,
            "next_vtable_symbol_starts": [f"0x{x:x}" for x in next_starts],
            "dispatch_slot_offset": slot_hex,
            "slot_address": f"0x{address_point + slot_offset:x}",
            "class_slot_key": f"{class_name}@slot={slot_hex}",
            "memory_pointer_observation_count": len(rows),
            "candidate_target_ids": target_ids,
            "candidate_target_count": len(target_ids),
            "target_functions": target_functions,
            "static_status": (
                "single_function_memory_pointer"
                if len(target_ids) == 1
                else "ambiguous_function_memory_pointers"
            ),
            "runtime_validation_status": runtime_validation,
            "runtime_target_ids": sorted(runtime_targets) if runtime_targets is not None else [],
            "runtime_verified": runtime_validation == "runtime_match",
            "target_transfer_scope": "same_receiver_vtable_only",
            "cross_vtable_transfer_allowed": False,
            "cross_class_transfer_allowed": False,
            "static_candidate_affects_routing": False,
            "source_kind": "ghidra_program_memory_pointer",
        })

    out.sort(key=lambda row: (row["receiver_vtable_id"], int(row["dispatch_slot_offset"], 0)))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in out:
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    print("format:", FORMAT)
    print("input memory-pointer rows:", input_rows)
    print("decoded vtables with resolved slots:", len(decoded_vtables))
    print("static function-slot candidates:", len(out))
    print("single-function memory pointers:", sum(x["static_status"] == "single_function_memory_pointer" for x in out))
    print("ambiguous memory pointers:", sum(x["static_status"] == "ambiguous_function_memory_pointers" for x in out))
    print("runtime matches:", sum(x["runtime_validation_status"] == "runtime_match" for x in out))
    print("runtime mismatches:", sum(x["runtime_validation_status"] == "runtime_mismatch" for x in out))
    print("runtime unobserved:", sum(x["runtime_validation_status"] == "runtime_unobserved" for x in out))
    print("rejected layout rows:", rejected_layout_rows)
    print("static candidates affect routing: no")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
