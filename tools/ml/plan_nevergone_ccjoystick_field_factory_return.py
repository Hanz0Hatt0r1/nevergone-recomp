#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any

OWNER_FORMAT = "NEVERGONE.ParameterRelativeReceiverFieldAudit/2"
STORE_FORMAT = "NEVERGONE.GhidraExactParameterFieldFactoryStore/1"
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


def main() -> int:
    p = argparse.ArgumentParser(description="Plan constructor-return proof for CCJoyStick +0x104 factory store")
    p.add_argument("owner_audit_jsonl", type=Path)
    p.add_argument("field_store_tsv", type=Path)
    p.add_argument("output_tsv", type=Path)
    a = p.parse_args()

    owner_rows = read_jsonl(a.owner_audit_jsonl)
    selected = [r for r in owner_rows if r.get("source_id") == SOURCE and r.get("call_index") == CALL_INDEX]
    if len(selected) != 1:
        raise ValueError("expected exactly one parameter-relative owner row")
    owner = selected[0]
    if owner.get("format") != OWNER_FORMAT:
        raise ValueError(f"expected {OWNER_FORMAT}")
    if owner.get("status") != "parameter_relative_field_with_function_owner_evidence":
        raise ValueError("parameter-relative owner evidence not accepted")
    if owner.get("ghidra_owner_class") != OWNER:
        raise ValueError("unexpected parameter owner class")
    if parse_int(owner.get("field_offset")) != FIELD:
        raise ValueError("unexpected parameter-relative field offset")
    if owner.get("dispatch_slot_offset") != SLOT:
        raise ValueError("unexpected dispatch slot")

    rows = read_tsv(a.field_store_tsv)
    for line_no, row in enumerate(rows, 2):
        if row.get("format") != STORE_FORMAT:
            raise ValueError(f"{a.field_store_tsv}:{line_no}: expected {STORE_FORMAT}")
    matches = [
        r for r in rows
        if (r.get("function_entry") or "").lower() == INIT_ENTRY
        and (r.get("function_name") or "").startswith("CCJoyStick::init")
        and r.get("base_parameter_index") == "0"
        and parse_int(r.get("address_offset")) == FIELD
    ]
    if len(matches) != 1:
        raise ValueError(f"expected exactly one CCJoyStick::init store to +0x{FIELD:x}, got {len(matches)}")
    store = matches[0]
    if store.get("value_root_opcode") != "CALL":
        raise ValueError("CCJoyStick +0x104 store is not rooted in direct CALL")
    callee_entry = store.get("factory_target_entry") or "-"
    callee_name = store.get("factory_target_name") or "-"
    if callee_entry == "-" or callee_name == "-":
        raise ValueError("factory target unresolved")

    a.output_tsv.parent.mkdir(parents=True, exist_ok=True)
    fields = ["source_id", "call_index", "source_entry", "call_address", "dispatch_slot_offset", "callee_entry", "callee_name"]
    row = {
        "source_id": SOURCE,
        "call_index": CALL_INDEX,
        "source_entry": "002a647e",
        "call_address": owner.get("call_address") or "-",
        "dispatch_slot_offset": SLOT,
        "callee_entry": callee_entry,
        "callee_name": callee_name,
    }
    with a.output_tsv.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, delimiter="\t")
        w.writeheader(); w.writerow(row)

    print("format: NEVERGONE.CCJoyStickFieldFactoryReturnPlan/1")
    print("source:", SOURCE, "call", CALL_INDEX, "slot=", SLOT)
    print("owner:", OWNER, "field=", f"0x{FIELD:x}")
    print("init store:", store.get("op_address"), "root=", store.get("value_root_token"))
    print("factory:", callee_entry, callee_name)
    print("output:", a.output_tsv)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
