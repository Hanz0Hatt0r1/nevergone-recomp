#!/usr/bin/env python3
"""Prove the BeatenEffect callback retention path and addChild forwarding.

This identifies a real retained owner alias. It does not promote the remaining
receiver classes or infer callback effects from a function name.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from audit_nevergone_beaten_factory_fields_v26 import definitions, inputs, rows, unwrap

FORMAT = "NEVERGONE.BeatenCallbackAliasAuditV29/1"
ROOT = "register:register:00000020:4"
TARGET = "register:register:00000024:4"


def get(data: list[dict[str, str]], address: str, opcode: str) -> dict[str, str]:
    found = [r for r in data if r["op_address"] == address and r["opcode"] == opcode]
    if len(found) != 1:
        raise ValueError(f"expected one {opcode} at {address}, got {len(found)}")
    return found[0]


def catalog_slot(path: Path, class_name: str, slot: str, target: str, name: str) -> dict[str, str]:
    found = []
    with path.open(encoding="utf-8") as stream:
        for line in stream:
            if not line.strip():
                continue
            r = json.loads(line)
            if r.get("class_name") == class_name and r.get("dispatch_slot_offset") == slot:
                found.append(r)
    if len(found) != 1:
        raise ValueError(f"expected one {class_name} slot {slot} catalog row")
    r = found[0]
    if (r.get("candidate_target_ids") != [target] or r.get("candidate_target_count") != 1
            or [x.get("name") for x in r.get("target_functions", [])] != [name]
            or r.get("runtime_validation_status") != "runtime_unobserved"):
        raise ValueError(f"changed {class_name} slot {slot} catalog candidate")
    return {"class_name": class_name, "slot": slot, "static_target_id": target,
            "static_target_name": name, "runtime_validation_status": r["runtime_validation_status"]}


def audit(pcode_dir: Path, catalog: Path) -> dict[str, object]:
    create = rows(pcode_dir / "pcode-0051f3c8.tsv")
    constructor = rows(pcode_dir / "pcode-0051eec0.tsv")
    init = rows(pcode_dir / "pcode-0051ea1c.tsv")
    wrapper = rows(pcode_dir / "pcode-00525dac.tsv")
    add_child = rows(pcode_dir / "pcode-00526c64.tsv")
    if [{r["function_entry"] for r in data} for data in (create, constructor, init, wrapper, add_child)] != [
        {"0051f3c8"}, {"0051eec0"}, {"0051ea1c"}, {"00525dac"}, {"00526c64"}
    ]:
        raise ValueError("p-code source function changed")

    ctor_call = get(create, "0051f3d8", "CALL")
    if ctor_call["callee_entry"] != "0051eec0" or ctor_call["callee_name"] != "cocos2d::CCCallFunc::CCCallFunc":
        raise ValueError("CCCallFunc constructor chain changed")
    ctor_vptr = get(constructor, "0051eed2", "STORE")
    if inputs(ctor_vptr) != ["const:0x1a1:4", "def:0051eed2:83:4", "def:0051eed0:19:4"]:
        raise ValueError("CCCallFunc constructor vptr store changed")
    ctor_null = get(constructor, "0051eed6", "STORE")
    if inputs(ctor_null) != ["const:0x1a1:4", "def:0051eed6:84:4", "const:0x0:4"]:
        raise ValueError("CCCallFunc constructor target initialization changed")
    _, ctor_target_address = unwrap("def:0051eed6:84:4", definitions(constructor))
    if (ctor_target_address is None or ctor_target_address["opcode"] != "PTRADD"
            or inputs(ctor_target_address) != [ROOT, "const:0x24:4", "const:0x1:4"]):
        raise ValueError("CCCallFunc constructor target field changed")
    create_call = get(create, "0051f3e6", "CALLIND")
    if inputs(create_call) != ["def:0051f3d2:28:4", ROOT]:
        raise ValueError("CCCallFunc::create owner forwarding changed")
    create_slot = get(create, "0051f3e4", "INT_ADD")
    if inputs(create_slot) != ["def:0051f3de:37:4", "const:0x38:4"]:
        raise ValueError("CCCallFunc::create virtual slot changed")
    callback_target = catalog_slot(catalog, "cocos2d::CCCallFunc", "0x38",
                                   "libcocos2dcpp.so:0051ea1c", "cocos2d::CCCallFunc::initWithTarget")

    retain = get(init, "0051ea26", "CALL")
    if retain["callee_name"] != "cocos2d::CCObject::retain" or inputs(retain) != [TARGET]:
        raise ValueError("callback owner retain changed")
    store = get(init, "0051ea32", "STORE")
    d = definitions(init)
    parts = inputs(store)
    if len(parts) != 3:
        raise ValueError("callback owner STORE arity changed")
    _, address = unwrap(parts[1], d)
    if (parts[2] != TARGET or address is None or address["opcode"] != "PTRADD"
            or inputs(address) != [ROOT, "const:0x24:4", "const:0x1:4"]):
        raise ValueError("callback owner field store changed")

    wrapper_slot = get(wrapper, "00525db0", "INT_ADD")
    if inputs(wrapper_slot) != ["def:00525dac:1:4", "const:0xc0:4"]:
        raise ValueError("CCNode::addChild wrapper slot changed")
    get(wrapper, "00525db8", "CALLIND")
    add_child_target = catalog_slot(catalog, "BeatenEffect", "0xc0",
                                    "libcocos2dcpp.so:00526c64", "cocos2d::CCNode::addChild")
    direct = {(r["callee_entry"], r["callee_name"]) for r in add_child if r["opcode"] == "CALL"}
    if not {("00526b5c", "cocos2d::CCNode::childrenAlloc"),
            ("00526c42", "cocos2d::CCNode::insertChild")} <= direct:
        raise ValueError(f"CCNode::addChild child-management calls changed: {direct!r}")
    return {"format": FORMAT,
            "callback_create_to_init_static_candidate": callback_target,
            "callback_owner_argument_forwarded": True, "callback_owner_retain_call": "0051ea26",
            "callback_owner_stored_at_offset": "0x24", "retained_owner_alias_static_path_proven": True,
            "callback_dispatch_runtime_observed": False,
            "add_child_wrapper_to_implementation_static_candidate": add_child_target,
            "add_child_implementation_calls": ["cocos2d::CCNode::childrenAlloc", "cocos2d::CCNode::insertChild"],
            "alias_frontier_closed": False, "receiver_promotions": 0, "affects_routing": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcode_dir", type=Path)
    parser.add_argument("vtable_catalog", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.pcode_dir, args.vtable_catalog)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("callback retained owner alias static path: proven; addChild forwarded slot: 0xc0; receiver promotions: 0")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
