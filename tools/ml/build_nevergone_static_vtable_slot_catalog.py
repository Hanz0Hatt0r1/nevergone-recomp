#!/usr/bin/env python3
"""Build bounded static vtable-slot candidates from existing Ghidra metadata.

The tool uses only explicit `_ZTV...`/demangled vtable symbols, ARM32 Itanium
address-point +8 layout, exact xrefs whose source is address_point + 4*n, and
known function-entry destinations. Static candidates are never treated as runtime
verified. An optional runtime class-dispatch catalog is used only to classify
observed slots as runtime_match/runtime_mismatch/runtime_unobserved.
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

FORMAT = "NEVERGONE.StaticVtableSlotCatalog/1"
MODULE = "libcocos2dcpp.so"
POINTER_SIZE = 4
DEFAULT_MAX_SLOT_OFFSET = 0x100


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
    parser.add_argument("symbols_tsv_gz", type=Path)
    parser.add_argument("xrefs_tsv_gz", type=Path)
    parser.add_argument("functions_tsv_gz", type=Path)
    parser.add_argument("--runtime-catalog", type=Path,
                        help="optional NEVERGONE.ClassDispatchCatalog JSONL used only for validation")
    parser.add_argument("--max-slot-offset", type=lambda x: int(x, 0), default=DEFAULT_MAX_SLOT_OFFSET)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    if args.max_slot_offset < 0 or args.max_slot_offset > 0x1000 or args.max_slot_offset % POINTER_SIZE:
        raise ValueError("--max-slot-offset must be a 4-byte aligned value in 0..0x1000")

    functions: dict[int, dict[str, Any]] = {}
    with open_tsv(args.functions_tsv_gz) as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            entry = addr(row.get("entry"))
            if entry is None:
                continue
            functions[entry] = {
                "entry": f"{entry:08x}",
                "name": row.get("name"),
                "signature": row.get("signature"),
                "address_count": row.get("address_count"),
                "is_external": row.get("is_external"),
            }

    vtables: dict[tuple[int, str], dict[str, Any]] = {}
    with open_tsv(args.symbols_tsv_gz) as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            start = addr(row.get("address"))
            name = str(row.get("name") or "")
            if start is None:
                continue
            class_name, evidence_kind = symbol_class_name(name)
            if not class_name or evidence_kind is None:
                continue
            address_point = start + ITANIUM_ARM32_ADDRESS_POINT_DELTA
            key = (address_point, class_name)
            candidate = {
                "vtable_symbol_start": start,
                "address_point": address_point,
                "class_name": class_name,
                "symbol_name": name,
                "symbol_type": row.get("type"),
                "symbol_source": row.get("source"),
                "symbol_is_primary": row.get("is_primary"),
                "class_evidence_kind": evidence_kind,
            }
            previous = vtables.get(key)
            if previous is None or str(row.get("is_primary")).lower() == "true":
                vtables[key] = candidate

    interesting_from: dict[int, tuple[int, str, int]] = {}
    for (address_point, class_name), _vtable in vtables.items():
        for slot_offset in range(0, args.max_slot_offset + POINTER_SIZE, POINTER_SIZE):
            interesting_from[address_point + slot_offset] = (address_point, class_name, slot_offset)

    xrefs_by_from: dict[int, list[dict[str, Any]]] = defaultdict(list)
    with open_tsv(args.xrefs_tsv_gz) as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            source = addr(row.get("from"))
            if source not in interesting_from:
                continue
            target = addr(row.get("to"))
            if target is None or target not in functions:
                continue
            xrefs_by_from[source].append({
                "from": row.get("from"),
                "to": row.get("to"),
                "type": row.get("type"),
                "operand": row.get("operand"),
                "source": row.get("source"),
                "target_entry": target,
            })

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

    out: list[dict[str, Any]] = []
    for (address_point, class_name), vtable in sorted(vtables.items()):
        vtable_id = f"{MODULE}:{address_point:08x}"
        for slot_offset in range(0, args.max_slot_offset + POINTER_SIZE, POINTER_SIZE):
            slot_address = address_point + slot_offset
            refs = xrefs_by_from.get(slot_address, [])
            if not refs:
                continue

            target_entries = sorted({int(ref["target_entry"]) for ref in refs})
            target_ids = [function_id(entry) for entry in target_entries]
            target_functions = []
            for entry in target_entries:
                fn = functions[entry]
                target_functions.append({
                    "target_id": function_id(entry),
                    "entry": fn["entry"],
                    "name": fn["name"],
                    "signature": fn["signature"],
                    "address_count": fn["address_count"],
                    "is_external": fn["is_external"],
                })

            static_status = "single_function_xref" if len(target_ids) == 1 else "ambiguous_function_xrefs"
            slot_hex = f"0x{slot_offset:x}"
            runtime_targets = runtime_by_key.get((vtable_id, slot_hex))
            if runtime_targets is None:
                runtime_validation = "runtime_unobserved"
            elif set(target_ids) == runtime_targets:
                runtime_validation = "runtime_match"
            else:
                runtime_validation = "runtime_mismatch"

            out.append({
                "format": FORMAT,
                "module": MODULE,
                "class_name": class_name,
                "class_evidence_kind": vtable["class_evidence_kind"],
                "vtable_symbol_name": vtable["symbol_name"],
                "vtable_symbol_start": f"0x{vtable['vtable_symbol_start']:x}",
                "receiver_vtable_address_point": f"0x{address_point:x}",
                "receiver_vtable_id": vtable_id,
                "address_point_delta": ITANIUM_ARM32_ADDRESS_POINT_DELTA,
                "dispatch_slot_offset": slot_hex,
                "slot_address": f"0x{slot_address:x}",
                "class_slot_key": f"{class_name}@slot={slot_hex}",
                "xref_count": len(refs),
                "xrefs": [{k: v for k, v in ref.items() if k != "target_entry"} for ref in refs],
                "candidate_target_ids": target_ids,
                "candidate_target_count": len(target_ids),
                "target_functions": target_functions,
                "static_status": static_status,
                "runtime_validation_status": runtime_validation,
                "runtime_target_ids": sorted(runtime_targets) if runtime_targets is not None else [],
                "runtime_verified": runtime_validation == "runtime_match",
                "target_transfer_scope": "same_receiver_vtable_only",
                "cross_vtable_transfer_allowed": False,
                "cross_class_transfer_allowed": False,
                "static_candidate_affects_routing": False,
            })

    out.sort(key=lambda row: (row["receiver_vtable_id"], int(row["dispatch_slot_offset"], 0)))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in out:
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    print("format:", FORMAT)
    print("decoded vtables:", len(vtables))
    print("static function-slot candidates:", len(out))
    print("single-function xrefs:", sum(x["static_status"] == "single_function_xref" for x in out))
    print("ambiguous function xrefs:", sum(x["static_status"] == "ambiguous_function_xrefs" for x in out))
    print("runtime matches:", sum(x["runtime_validation_status"] == "runtime_match" for x in out))
    print("runtime mismatches:", sum(x["runtime_validation_status"] == "runtime_mismatch" for x in out))
    print("runtime unobserved:", sum(x["runtime_validation_status"] == "runtime_unobserved" for x in out))
    print("static candidates affect routing: no")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
