#!/usr/bin/env python3
"""Audit BeatenEffect's inherited addChild path and child parent alias.

The static path stores the owner pointer in a child node. This is an alias
barrier, not a reason to promote a receiver class.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from audit_nevergone_beaten_factory_fields_v26 import definitions, inputs, rows, unwrap

FORMAT = "NEVERGONE.BeatenInheritedOwnerAuditV32/1"
ROOT = "register:register:00000020:4"
CHILD = "register:register:00000024:4"


def one(data: list[dict[str, str]], address: str, opcode: str) -> dict[str, str]:
    matches = [r for r in data if r["op_address"] == address and r["opcode"] == opcode]
    if len(matches) != 1:
        raise ValueError(f"expected one {opcode} at {address}, got {len(matches)}")
    return matches[0]


def check_store(data: list[dict[str, str]], address: str, base: str, offset: int, value: str) -> None:
    store = one(data, address, "STORE")
    parts = inputs(store)
    if len(parts) != 3 or parts[2] != value:
        raise ValueError(f"STORE value changed at {address}")
    _, address_op = unwrap(parts[1], definitions(data))
    if (address_op is None or address_op["opcode"] != "PTRADD"
            or inputs(address_op) != [base, f"const:0x{offset:x}:4", "const:0x1:4"]):
        raise ValueError(f"STORE address changed at {address}")


def child_set_parent_candidate(catalog: Path) -> dict[str, str]:
    found = []
    with catalog.open(encoding="utf-8") as stream:
        for line in stream:
            if not line.strip():
                continue
            r = json.loads(line)
            if r.get("class_name") == "cocos2d::CCSprite" and r.get("dispatch_slot_offset") == "0xc8":
                found.append(r)
    if len(found) != 1:
        raise ValueError("expected one CCSprite slot 0xc8 catalog row")
    r = found[0]
    if (r.get("candidate_target_ids") != ["libcocos2dcpp.so:00525d4c"]
            or [t.get("name") for t in r.get("target_functions", [])] != ["cocos2d::CCNode::setParent"]
            or r.get("runtime_validation_status") != "runtime_unobserved"):
        raise ValueError("CCSprite setParent static candidate changed")
    return {"child_class": "cocos2d::CCSprite", "slot": "0xc8",
            "target_id": "libcocos2dcpp.so:00525d4c", "target_name": "cocos2d::CCNode::setParent",
            "runtime_validation_status": "runtime_unobserved"}


def audit(pcode_dir: Path, add_child_pcode: Path, catalog: Path, v29_report: Path) -> dict[str, object]:
    previous = json.loads(v29_report.read_text(encoding="utf-8"))
    if (previous.get("format") != "NEVERGONE.BeatenCallbackAliasAuditV29/1"
            or previous.get("add_child_wrapper_to_implementation_static_candidate", {}).get("static_target_id")
            != "libcocos2dcpp.so:00526c64"):
        raise ValueError("v29 addChild chain not established")
    alloc = rows(pcode_dir / "pcode-00526b5c.tsv")
    insert = rows(pcode_dir / "pcode-00526c42.tsv")
    impl = rows(add_child_pcode)
    parent = rows(pcode_dir / "pcode-00525d4c.tsv")
    if [{r["function_entry"] for r in data} for data in (alloc, insert, impl, parent)] != [
        {"00526b5c"}, {"00526c42"}, {"00526c64"}, {"00525d4c"}
    ]:
        raise ValueError("inherited p-code source function changed")
    alloc_factory = one(alloc, "00526b62", "CALL")
    if alloc_factory["callee_name"] != "cocos2d::CCArray::createWithCapacity":
        raise ValueError("childrenAlloc factory changed")
    alloc_store = one(alloc, "00526b66", "STORE")
    _, alloc_value = unwrap(inputs(alloc_store)[2], definitions(alloc))
    if alloc_value != alloc_factory:
        raise ValueError("childrenAlloc stored value changed")
    check_store(alloc, "00526b66", ROOT, 0xA4, inputs(alloc_store)[2])
    check_store(insert, "00526c46", ROOT, 0xD2, "const:0x1:1")
    alloc_call = one(impl, "00526c74", "CALL")
    insert_call = one(impl, "00526c7e", "CALL")
    if (alloc_call["callee_entry"] != "00526b5c" or inputs(alloc_call) != [ROOT]
            or insert_call["callee_entry"] != "00526c42" or inputs(insert_call)[:2] != [ROOT, CHILD]):
        raise ValueError("addChild implementation call chain changed")
    child_dispatch = one(impl, "00526c90", "CALLIND")
    slot = one(impl, "00526c8c", "INT_ADD")
    if inputs(child_dispatch) != [CHILD, ROOT] or inputs(slot)[-1] != "const:0xc8:4":
        raise ValueError("child setParent dispatch changed")
    candidate = child_set_parent_candidate(catalog)
    check_store(parent, "00525d4c", ROOT, 0xA8, CHILD)
    return {"format": FORMAT, "direct_inherited_owner_writes": [
                {"function": "cocos2d::CCNode::childrenAlloc", "offset": "0xa4", "value": "CCArray factory"},
                {"function": "cocos2d::CCNode::insertChild", "offset": "0xd2", "value": "1"}],
            "child_virtual_parent_setter_static_candidate": candidate,
            "child_parent_field_offset": "0xa8", "child_stores_owner_pointer_static_path": True,
            "alias_frontier_closed": False, "receiver_promotions": 0, "affects_routing": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcode_dir", type=Path)
    parser.add_argument("add_child_pcode", type=Path)
    parser.add_argument("vtable_catalog", type=Path)
    parser.add_argument("v29_report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.pcode_dir, args.add_child_pcode, args.vtable_catalog, args.v29_report)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("inherited owner writes: +0xa4, +0xd2; child parent alias: +0xa8; receiver promotions: 0")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
