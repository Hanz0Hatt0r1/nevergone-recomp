#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any

OWNER_FORMAT = "NEVERGONE.ParameterRelativeReceiverFieldAudit/2"
STORE_FORMAT = "NEVERGONE.GhidraExactParameterFieldFactoryStore/1"
FACTORY_FORMAT = "NEVERGONE.CallRootFactoryReturnAudit/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
OUT_FORMAT = "NEVERGONE.CCJoyStickFieldFactoryProvenanceAudit/1"
SOURCE = "libcocos2dcpp.so:002a647e"
CALL_INDEX = 4
SLOT = "0x58"
OWNER = "CCJoyStick"
FIELD = 0x104
INIT_ENTRY = "002a6584"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def parse_int(value: Any) -> int | None:
    if value in (None, "", "-"):
        return None
    try:
        return int(str(value), 0)
    except ValueError:
        return None


def norm_slot(value: Any) -> str | None:
    try:
        return f"0x{int(str(value), 0):x}"
    except Exception:
        return None


def main() -> int:
    p = argparse.ArgumentParser(description="Audit CCJoyStick +0x104 factory/constructor receiver provenance")
    p.add_argument("owner_audit_jsonl", type=Path)
    p.add_argument("field_store_tsv", type=Path)
    p.add_argument("factory_audit_jsonl", type=Path)
    p.add_argument("catalog_jsonl", type=Path)
    p.add_argument("--details-output", type=Path)
    a = p.parse_args()

    reasons: list[str] = []

    owners = [r for r in read_jsonl(a.owner_audit_jsonl) if r.get("source_id") == SOURCE and r.get("call_index") == CALL_INDEX]
    owner = owners[0] if owners else {}
    if len(owners) != 1: reasons.append("owner_evidence_not_unique")
    if owner.get("format") != OWNER_FORMAT: reasons.append("owner_format_mismatch")
    if owner.get("status") != "parameter_relative_field_with_function_owner_evidence": reasons.append("owner_evidence_not_accepted")
    if owner.get("ghidra_owner_class") != OWNER: reasons.append("owner_class_mismatch")
    if parse_int(owner.get("field_offset")) != FIELD: reasons.append("owner_field_offset_mismatch")
    if norm_slot(owner.get("dispatch_slot_offset")) != SLOT: reasons.append("owner_slot_mismatch")

    stores = read_tsv(a.field_store_tsv)
    for line_no, row in enumerate(stores, 2):
        if row.get("format") != STORE_FORMAT:
            raise ValueError(f"{a.field_store_tsv}:{line_no}: expected {STORE_FORMAT}")
    field_stores = [
        r for r in stores
        if (r.get("function_entry") or "").lower() == INIT_ENTRY
        and (r.get("function_name") or "").startswith("CCJoyStick::init")
        and r.get("base_parameter_index") == "0"
        and parse_int(r.get("address_offset")) == FIELD
    ]
    store = field_stores[0] if field_stores else {}
    if len(field_stores) != 1: reasons.append("field_store_not_unique")
    if store.get("value_root_opcode") != "CALL": reasons.append("field_store_root_not_direct_call")
    factory_entry = store.get("factory_target_entry") or None
    factory_name = store.get("factory_target_name") or None
    if factory_entry in (None, "-") or factory_name in (None, "-"): reasons.append("field_factory_target_unresolved")

    factory_rows = [r for r in read_jsonl(a.factory_audit_jsonl) if r.get("source_id") == SOURCE and r.get("call_index") == CALL_INDEX]
    factory = factory_rows[0] if factory_rows else {}
    if len(factory_rows) != 1: reasons.append("factory_audit_not_unique")
    if factory.get("format") != FACTORY_FORMAT: reasons.append("factory_audit_format_mismatch")
    if factory.get("status") != "unique_factory_return_class": reasons.append("factory_return_class_not_unique")
    if factory_entry and factory.get("callee_entry") != factory_entry: reasons.append("factory_callee_entry_mismatch")
    if factory_name and factory.get("callee_name") != factory_name: reasons.append("factory_callee_name_mismatch")
    classes = factory.get("candidate_receiver_class_names") if isinstance(factory.get("candidate_receiver_class_names"), list) else []
    vtables = factory.get("candidate_receiver_vtable_ids") if isinstance(factory.get("candidate_receiver_vtable_ids"), list) else []
    if len(classes) != 1: reasons.append("factory_candidate_class_count_mismatch")
    if len(vtables) != 1: reasons.append("factory_candidate_vtable_count_mismatch")
    chosen_class = classes[0] if len(classes) == 1 else None
    chosen_vtable = vtables[0] if len(vtables) == 1 else None

    catalog = read_jsonl(a.catalog_jsonl)
    for line_no, row in enumerate(catalog, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{a.catalog_jsonl}:{line_no}: expected {CATALOG_FORMAT}")
    slot_rows = [
        r for r in catalog
        if r.get("class_name") == chosen_class
        and r.get("receiver_vtable_id") == chosen_vtable
        and norm_slot(r.get("dispatch_slot_offset")) == SLOT
    ] if chosen_class and chosen_vtable else []
    slot_row = slot_rows[0] if slot_rows else {}
    if len(slot_rows) != 1: reasons.append("factory_class_slot_catalog_row_not_unique")
    if slot_row.get("static_status") != "single_function_memory_pointer": reasons.append("factory_class_slot_not_single_function")
    targets = slot_row.get("candidate_target_ids") if isinstance(slot_row.get("candidate_target_ids"), list) else []
    if len(targets) != 1: reasons.append("factory_class_slot_target_not_unique")
    target_names = [x.get("name") for x in (slot_row.get("target_functions") or []) if isinstance(x, dict) and x.get("name")]

    status = "ccjoystick_field_factory_receiver_unique_class" if not reasons else "ccjoystick_field_factory_receiver_incomplete"
    detail = {
        "format": OUT_FORMAT,
        "source_id": SOURCE,
        "call_index": CALL_INDEX,
        "call_address": owner.get("call_address"),
        "dispatch_slot_offset": SLOT,
        "owner_class": OWNER,
        "field_offset": FIELD,
        "init_function_entry": INIT_ENTRY,
        "field_store_address": store.get("op_address"),
        "field_store_value_root_token": store.get("value_root_token"),
        "factory_callee_entry": factory_entry,
        "factory_callee_name": factory_name,
        "factory_return_class": chosen_class,
        "factory_return_vtable_id": chosen_vtable,
        "slot_target_ids": targets,
        "slot_target_names": target_names,
        "runtime_validation_status": slot_row.get("runtime_validation_status"),
        "status": status,
        "reasons": reasons,
        "affects_routing": False,
    }

    print("format:", OUT_FORMAT)
    print("source:", SOURCE, "call", CALL_INDEX, "slot=", SLOT)
    print("owner:", OWNER, "field=", f"0x{FIELD:x}")
    print("field store:", detail["field_store_address"], "root=", detail["field_store_value_root_token"])
    print("factory:", factory_entry, factory_name)
    print("factory return class:", chosen_class)
    print("factory return vtable:", chosen_vtable)
    print("slot targets:", targets)
    print("slot names:", target_names)
    print("runtime:", detail["runtime_validation_status"])
    print("status:", status)
    print("reasons:", reasons)
    print("affects routing: no")
    if a.details_output:
        a.details_output.parent.mkdir(parents=True, exist_ok=True)
        a.details_output.write_text(json.dumps(detail, sort_keys=True) + "\n", encoding="utf-8")
        print("details output:", a.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
