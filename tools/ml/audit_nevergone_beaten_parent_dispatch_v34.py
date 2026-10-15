#!/usr/bin/env python3
"""Audit static BeatenEffect parent dispatch and direct getParent references."""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

from audit_nevergone_beaten_factory_fields_v26 import definitions, inputs, rows, unwrap

FORMAT = "NEVERGONE.BeatenParentDispatchV34/1"
ROOT = "register:register:00000020:4"
CHILD = "register:register:00000024:4"
SLOTS = {
    "0xcc": ("libcocos2dcpp.so:00525d46", "cocos2d::CCNode::getParent"),
    "0xdc": ("libcocos2dcpp.so:00526c1e", "cocos2d::CCNode::removeChild"),
    "0xf0": ("libcocos2dcpp.so:00525e20", "cocos2d::CCNode::reorderChild"),
}


def table(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def one(data: list[dict[str, str]], address: str, opcode: str) -> dict[str, str]:
    found = [op for op in data if op["op_address"] == address and op["opcode"] == opcode]
    if len(found) != 1:
        raise ValueError(f"expected one {opcode} at {address}, got {len(found)}")
    return found[0]


def check_dispatch(data: list[dict[str, str]], slot_address: str, call_address: str,
                   slot: str, receiver: str) -> None:
    offset = one(data, slot_address, "INT_ADD")
    if inputs(offset)[-1] != f"const:{slot}:4":
        raise ValueError(f"virtual slot changed at {slot_address}")
    call = one(data, call_address, "CALLIND")
    if receiver not in inputs(call):
        raise ValueError(f"virtual receiver changed at {call_address}")


def audit(evidence: Path, prior_evidence: Path, catalog: Path, v33_report: Path) -> dict[str, object]:
    v33 = json.loads(v33_report.read_text(encoding="utf-8"))
    if (v33.get("format") != "NEVERGONE.BeatenChildParentReadsV33/1"
            or v33.get("candidate_load_count") != 7 or v33.get("direct_parent_derived_store_count") != 0):
        raise ValueError("v33 parent-read scope changed")
    consumer = table(evidence / "getparent-consumers.tsv")
    if (len(consumer) != 1 or consumer[0]["row_type"] != "summary"
            or consumer[0]["accessor_entry"] != "00525d46"
            or consumer[0]["direct_caller_functions"] != "0"
            or consumer[0]["accessor_call_sites"] != "0"
            or consumer[0]["status"] != "no_direct_callers"):
        raise ValueError("getParent direct caller evidence changed")
    references = table(evidence / "getparent-references.tsv")
    if (len(references) != 1 or references[0]["target_address"] != "00525d46"
            or references[0]["reference_from"] != "Entry Point"):
        raise ValueError("getParent reference evidence changed")
    selected = {}
    with catalog.open(encoding="utf-8") as stream:
        for line in stream:
            if not line.strip():
                continue
            row = json.loads(line)
            slot = row.get("dispatch_slot_offset")
            if row.get("class_name") == "BeatenEffect" and slot in SLOTS:
                selected.setdefault(slot, []).append(row)
    if set(selected) != set(SLOTS) or any(len(value) != 1 for value in selected.values()):
        raise ValueError("BeatenEffect parent slot catalog changed")
    slot_rows = []
    for slot, (target_id, name) in SLOTS.items():
        row = selected[slot][0]
        if (row.get("candidate_target_ids") != [target_id]
                or [target.get("name") for target in row.get("target_functions", [])] != [name]
                or row.get("runtime_validation_status") != "runtime_unobserved"):
            raise ValueError(f"BeatenEffect slot {slot} target changed")
        slot_rows.append({"slot": slot, "static_target_id": target_id,
                          "static_target_name": name, "runtime_status": "runtime_unobserved"})
    set_z = rows(prior_evidence / "pcode-00525c80.tsv")
    remove_parent = rows(prior_evidence / "pcode-00525dde.tsv")
    reorder_dirty = rows(prior_evidence / "pcode-0054934a.tsv")
    check_dispatch(set_z, "00525c98", "00525c9c", "0xf0", "def:00525c8c:23:4")
    check_dispatch(remove_parent, "00525dee", "00525df2", "0xdc", "def:00525de2:12:4")
    check_dispatch(reorder_dirty, "00549376", "0054937a", "0xcc", "def:0054935e:109:4")
    getter_call = one(reorder_dirty, "0054937a", "CALLIND")
    if getter_call["output"] != "def:0054937a:62:4":
        raise ValueError("virtual getParent return changed")
    remove_child = rows(evidence / "pcode-00526c1e.tsv")
    reorder_child = rows(evidence / "pcode-00525e20.tsv")
    detach_child = rows(evidence / "pcode-00526bd4.tsv")
    detach_call = one(remove_child, "00526c3c", "CALL")
    if (detach_call["callee_entry"] != "00526bd4"
            or inputs(detach_call) != [ROOT, CHILD, "register:register:00000028:1"]):
        raise ValueError("removeChild-to-detachChild path changed")
    store = one(reorder_child, "00525e24", "STORE")
    _, store_address = unwrap(inputs(store)[1], definitions(reorder_child))
    if (store_address is None or store_address["opcode"] != "PTRADD"
            or inputs(store_address) != [ROOT, "const:0xd2:4", "const:0x1:4"]
            or inputs(store)[2] != "const:0x1:1"):
        raise ValueError("reorderChild owner flag write changed")
    check_dispatch(detach_child, "00526c08", "00526c0c", "0xc8", CHILD)
    if inputs(one(detach_child, "00526c0c", "CALLIND")) != [CHILD, "const:0x0:4"]:
        raise ValueError("detachChild parent-clear argument changed")
    remove_object = one(detach_child, "00526c1a", "CALL")
    if (remove_object["callee_name"] != "cocos2d::CCArray::removeObject"
            or inputs(remove_object)[-1] != "const:0x1:1"):
        raise ValueError("detachChild array removal changed")
    return {"format": FORMAT, "direct_getparent_callers": 0,
            "getparent_non_entry_references_in_ghidra": 0,
            "static_virtual_getparent_path_present": True,
            "beaten_parent_virtual_slots": slot_rows,
            "reorder_child_owner_write": {"offset": "0xd2", "value": "1"},
            "remove_child_static_delegate": "cocos2d::CCNode::detachChild",
            "detach_child_static_actions": ["child slot 0xc8 called with null parent",
                                            "CCArray::removeObject called on owner child array"],
            "runtime_dispatch_observed": False, "alias_frontier_closed": False,
            "receiver_promotions": 0, "affects_routing": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("prior_evidence", type=Path)
    parser.add_argument("catalog", type=Path)
    parser.add_argument("v33_report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.evidence, args.prior_evidence, args.catalog, args.v33_report)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("direct getParent callers: 0; static virtual parent slots: 3; receiver promotions: 0")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
