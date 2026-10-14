#!/usr/bin/env python3
"""Audit constructor -> exact singleton cell STORE -> exact cell LOAD -> RETURN provenance.

Evidence-only. Candidate classes come only from class-qualified constructor calls
whose `this` root is exactly the value root written to the independently proven
singleton cell. Accessor names and nearby-class hints are never used as class
proof.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any

PROV_FORMAT = "NEVERGONE.GhidraExactSingletonCellProvenance/1"
OWNER_FORMAT = "NEVERGONE.LoadEffectiveAddressOwnerAudit/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
OUT_FORMAT = "NEVERGONE.ExactSingletonCellProvenanceAudit/1"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            rows.append(row)
    return rows


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def address_key(value: Any) -> str | None:
    if value in (None, "", "-"):
        return None
    if isinstance(value, int):
        return f"{value & 0xffffffff:08x}"
    text = str(value).strip().lower()
    if text.startswith("0x"):
        text = text[2:]
    try:
        return f"{int(text, 16) & 0xffffffff:08x}"
    except ValueError:
        return None


def dec(value: Any, default: int = -1) -> int:
    try:
        return int(str(value), 10)
    except Exception:
        return default


def constructor_class(name: Any) -> str | None:
    if not isinstance(name, str) or name in {"", "-"}:
        return None
    head = name.split("(", 1)[0].strip()
    # Drop common return/type prefixes if Ghidra ever renders them here.
    head = re.sub(r"^(?:void|undefined\d*|int|long|bool)\s+", "", head)
    parts = [part for part in head.split("::") if part]
    if len(parts) < 2 or parts[-1] != parts[-2]:
        return None
    return "::".join(parts[:-1])


def target_names(row: dict[str, Any]) -> list[str]:
    funcs = row.get("target_functions")
    names: list[str] = []
    if isinstance(funcs, list):
        for item in funcs:
            if isinstance(item, dict) and isinstance(item.get("name"), str):
                names.append(item["name"])
    return sorted(set(names))


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("owner_audit_jsonl", type=Path)
    p.add_argument("catalog_jsonl", type=Path)
    p.add_argument("provenance_tsv", type=Path, nargs="+")
    p.add_argument("--details-output", type=Path)
    a = p.parse_args()

    owner_rows = read_jsonl(a.owner_audit_jsonl)
    catalog = read_jsonl(a.catalog_jsonl)
    provenance: list[dict[str, str]] = []
    for path in a.provenance_tsv:
        rows = read_tsv(path)
        for line_no, row in enumerate(rows, 2):
            if row.get("format") != PROV_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {PROV_FORMAT}")
        provenance.extend(rows)

    for line_no, row in enumerate(owner_rows, 1):
        if row.get("format") != OWNER_FORMAT:
            raise ValueError(f"{a.owner_audit_jsonl}:{line_no}: expected {OWNER_FORMAT}")
    for line_no, row in enumerate(catalog, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{a.catalog_jsonl}:{line_no}: expected {CATALOG_FORMAT}")

    groups: dict[tuple[str, int], list[dict[str, str]]] = defaultdict(list)
    for row in provenance:
        source_id = row.get("source_id", "")
        call_index = dec(row.get("call_index"))
        if source_id and call_index >= 0:
            groups[(source_id, call_index)].append(row)

    details: list[dict[str, Any]] = []
    for (source_id, call_index), rows in sorted(groups.items()):
        reasons: list[str] = []
        cells = {address_key(row.get("cell_address")) for row in rows}
        cells.discard(None)
        functions = {row.get("function_name") for row in rows if row.get("function_name") not in {None, "", "-"}}
        entries = {address_key(row.get("function_entry")) for row in rows}
        entries.discard(None)

        if len(cells) != 1:
            reasons.append("provenance_cell_not_unique")
        if len(functions) != 1 or len(entries) != 1:
            reasons.append("provenance_function_not_unique")
        cell = next(iter(cells)) if len(cells) == 1 else None
        function_name = next(iter(functions)) if len(functions) == 1 else None
        function_entry = next(iter(entries)) if len(entries) == 1 else None

        owners = [
            row for row in owner_rows
            if row.get("source_id") == source_id and row.get("call_index") == call_index
        ]
        owner = owners[0] if len(owners) == 1 else None
        if owner is None:
            reasons.append("owner_audit_row_not_unique")
        else:
            if address_key(owner.get("effective_address")) != cell:
                reasons.append("owner_effective_cell_mismatch")
            if owner.get("status") != "writable_effective_cell_with_refs":
                reasons.append("owner_cell_not_writable_with_refs")
            owner_functions = owner.get("owner_functions")
            if not isinstance(owner_functions, list) or function_name not in owner_functions:
                reasons.append("accessor_not_exact_cell_xref_owner")

        calls = [row for row in rows if row.get("row_type") == "call"]
        stores = [row for row in rows if row.get("row_type") == "store" and row.get("exact_cell_reference") == "1"]
        loads = [row for row in rows if row.get("row_type") == "load" and row.get("exact_cell_reference") == "1"]
        returns = [row for row in rows if row.get("row_type") == "return"]
        store_roots = {row.get("value_root_token") for row in stores if row.get("value_root_token") not in {None, "", "-"}}
        load_roots = {row.get("value_root_token") for row in loads if row.get("value_root_token") not in {None, "", "-"}}
        return_roots = {row.get("value_root_token") for row in returns if row.get("value_root_token") not in {None, "", "-"}}

        ctor_matches: list[dict[str, Any]] = []
        for call in calls:
            cls = constructor_class(call.get("call_target_name"))
            arg0 = call.get("call_arg0_root_token")
            if cls is not None and arg0 in store_roots:
                ctor_matches.append({
                    "class_name": cls,
                    "call_target_entry": address_key(call.get("call_target_entry")),
                    "call_target_name": call.get("call_target_name"),
                    "object_root": arg0,
                    "op_address": address_key(call.get("op_address")),
                })

        classes = sorted({item["class_name"] for item in ctor_matches})
        returned_load_roots = sorted(load_roots & return_roots)
        if not stores:
            reasons.append("no_exact_cell_store")
        if not loads:
            reasons.append("no_exact_cell_load")
        if not returns:
            reasons.append("no_return")
        if len(classes) != 1:
            reasons.append("constructor_proven_store_class_not_unique")
        if len(returned_load_roots) < 1:
            reasons.append("exact_cell_load_not_returned")

        cls = classes[0] if len(classes) == 1 else None
        class_vtables = sorted({
            row.get("receiver_vtable_id") for row in catalog
            if cls is not None and row.get("class_name") == cls and isinstance(row.get("receiver_vtable_id"), str)
        })
        if cls is not None and len(class_vtables) != 1:
            reasons.append("constructor_class_vtable_not_unique")
        vtable = class_vtables[0] if len(class_vtables) == 1 else None

        slot = owner.get("dispatch_slot_offset") if owner else None
        slot_rows = [
            row for row in catalog
            if vtable is not None
            and row.get("receiver_vtable_id") == vtable
            and row.get("dispatch_slot_offset") == slot
        ]
        slot_target_ids: list[str] = []
        slot_target_names: list[str] = []
        slot_status: str | None = None
        if vtable is not None and isinstance(slot, str):
            if len(slot_rows) != 1:
                reasons.append("exact_vtable_slot_row_not_unique")
            else:
                slot_row = slot_rows[0]
                slot_status = slot_row.get("static_status")
                ids = slot_row.get("candidate_target_ids")
                if isinstance(ids, list):
                    slot_target_ids = [item for item in ids if isinstance(item, str)]
                slot_target_names = target_names(slot_row)
                if slot_status != "single_function_memory_pointer" or len(slot_target_ids) != 1:
                    reasons.append("exact_vtable_slot_target_not_unique")

        status = (
            "singleton_cell_constructor_store_return_unique_class"
            if not reasons and cls is not None and vtable is not None
            else "singleton_cell_class_not_proven"
        )
        detail = {
            "format": OUT_FORMAT,
            "source_id": source_id,
            "call_index": call_index,
            "dispatch_slot_offset": slot,
            "accessor_function_entry": function_entry,
            "accessor_function_name": function_name,
            "singleton_cell": cell,
            "exact_store_count": len(stores),
            "exact_load_count": len(loads),
            "return_count": len(returns),
            "store_roots": sorted(store_roots),
            "load_roots": sorted(load_roots),
            "return_roots": sorted(return_roots),
            "returned_exact_load_roots": returned_load_roots,
            "constructor_store_matches": ctor_matches,
            "candidate_classes": classes,
            "candidate_vtables": class_vtables,
            "slot_static_status": slot_status,
            "slot_target_ids": slot_target_ids,
            "slot_target_names": slot_target_names,
            "status": status,
            "reasons": reasons,
            "affects_routing": False,
        }
        details.append(detail)

    if a.details_output:
        a.details_output.parent.mkdir(parents=True, exist_ok=True)
        with a.details_output.open("w", encoding="utf-8") as handle:
            for row in details:
                handle.write(json.dumps(row, sort_keys=True) + "\n")

    print("format:", OUT_FORMAT)
    print("cases:", len(details))
    print("rows:")
    for row in details:
        print(
            row["source_id"], "call", row["call_index"],
            "slot=", row["dispatch_slot_offset"],
            "accessor=", row["accessor_function_name"],
            "cell=", row["singleton_cell"],
            "stores=", row["exact_store_count"],
            "loads=", row["exact_load_count"],
            "returns=", row["return_count"],
            "returned_load_roots=", row["returned_exact_load_roots"],
            "classes=", row["candidate_classes"],
            "vtables=", row["candidate_vtables"],
            "slot_targets=", row["slot_target_ids"],
            "slot_names=", row["slot_target_names"],
            "status=", row["status"],
        )
        if row["constructor_store_matches"]:
            print("  constructor/store matches:", row["constructor_store_matches"])
        if row["reasons"]:
            print("  reasons:", row["reasons"])
    if a.details_output:
        print("details output:", a.details_output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
