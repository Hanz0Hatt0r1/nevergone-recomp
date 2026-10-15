#!/usr/bin/env python3
"""Audit static CCSprite targets of BeatenEffect child management calls."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from audit_nevergone_beaten_factory_fields_v26 import definitions, inputs, rows, unwrap

FORMAT = "NEVERGONE.BeatenChildDispatchV35/1"
CHILD = "register:register:00000020:4"
ARG = "register:register:00000024:4"
TARGETS = {
    "0x2c": ("00525c7a", "cocos2d::CCNode::_setZOrder"),
    "0xa0": ("00525d8e", "cocos2d::CCNode::setOrderOfArrival"),
    "0xc8": ("00525d4c", "cocos2d::CCNode::setParent"),
    "0x138": ("005270f8", "cocos2d::CCNode::onExit"),
    "0x13c": ("00525fe0", "cocos2d::CCNode::onExitTransitionDidStart"),
    "0x140": ("00526fce", "cocos2d::CCNode::cleanup"),
}
SITES = [
    ("00525e20", "00525e32", "00525e40", "0xa0"),
    ("00525e20", "00525e48", "00525e4a", "0x2c"),
    ("00526bd4", "00526be6", "00526bea", "0x13c"),
    ("00526bd4", "00526bf0", "00526bf4", "0x138"),
    ("00526bd4", "00526bfc", "00526c00", "0x140"),
    ("00526bd4", "00526c08", "00526c0c", "0xc8"),
]


def one(data: list[dict[str, str]], address: str, opcode: str) -> dict[str, str]:
    found = [op for op in data if op["op_address"] == address and op["opcode"] == opcode]
    if len(found) != 1:
        raise ValueError(f"expected one {opcode} at {address}, got {len(found)}")
    return found[0]


def check_store(data: list[dict[str, str]], address: str, offset: int, value: str) -> None:
    store = one(data, address, "STORE")
    parts = inputs(store)
    if len(parts) != 3 or parts[2] != value:
        raise ValueError(f"STORE value changed at {address}")
    _, base = unwrap(parts[1], definitions(data))
    if (base is None or base["opcode"] != "PTRADD"
            or inputs(base) != [CHILD, f"const:0x{offset:x}:4", "const:0x1:4"]):
        raise ValueError(f"STORE destination changed at {address}")


def direct_parent_loads(data: list[dict[str, str]]) -> list[str]:
    defs = definitions(data)
    found = []
    for op in data:
        if op["opcode"] != "LOAD" or len(inputs(op)) != 2:
            continue
        _, address_op = unwrap(inputs(op)[1], defs)
        if (address_op is not None and address_op["opcode"] in {"PTRADD", "PTRSUB", "INT_ADD"}
                and "const:0xa8:4" in inputs(address_op)):
            found.append(op["op_address"])
    return found


def audit(prior_evidence: Path, target_evidence: Path, parent_evidence: Path,
          catalog: Path, v34_report: Path) -> dict[str, object]:
    previous = json.loads(v34_report.read_text(encoding="utf-8"))
    if (previous.get("format") != "NEVERGONE.BeatenParentDispatchV34/1"
            or previous.get("remove_child_static_delegate") != "cocos2d::CCNode::detachChild"):
        raise ValueError("v34 child management path changed")
    selected: dict[str, list[dict[str, object]]] = {}
    with catalog.open(encoding="utf-8") as stream:
        for line in stream:
            if not line.strip():
                continue
            row = json.loads(line)
            slot = row.get("dispatch_slot_offset")
            if row.get("class_name") == "cocos2d::CCSprite" and slot in TARGETS:
                selected.setdefault(slot, []).append(row)
    if set(selected) != set(TARGETS) or any(len(value) != 1 for value in selected.values()):
        raise ValueError("CCSprite child slot catalog changed")
    target_rows = []
    for slot, (entry, name) in TARGETS.items():
        row = selected[slot][0]
        if (row.get("candidate_target_ids") != [f"libcocos2dcpp.so:{entry}"]
                or [target.get("name") for target in row.get("target_functions", [])] != [name]
                or row.get("runtime_validation_status") != "runtime_unobserved"):
            raise ValueError(f"CCSprite slot {slot} target changed")
        target_rows.append({"slot": slot, "static_target_id": f"libcocos2dcpp.so:{entry}",
                            "static_target_name": name, "runtime_status": "runtime_unobserved"})
    caller_data = {entry: rows(prior_evidence / f"pcode-{entry}.tsv")
                   for entry in {site[0] for site in SITES}}
    for entry, address, call_address, slot in SITES:
        data = caller_data[entry]
        offset = one(data, address, "INT_ADD")
        if inputs(offset)[-1] != f"const:{slot}:4":
            raise ValueError(f"child dispatch slot changed at {address}")
        call = one(data, call_address, "CALLIND")
        if ARG not in inputs(call):
            raise ValueError(f"child dispatch receiver changed at {call_address}")
    if inputs(one(caller_data["00526bd4"], "00526c0c", "CALLIND")) != [ARG, "const:0x0:4"]:
        raise ValueError("detachChild null parent changed")
    target_data = {entry: rows(parent_evidence / "pcode-00525d4c.tsv") if entry == "00525d4c"
                   else rows(target_evidence / f"pcode-{entry}.tsv")
                   for entry, _ in TARGETS.values()}
    for entry, data in target_data.items():
        if {op["function_entry"] for op in data} != {entry}:
            raise ValueError(f"target p-code source changed: {entry}")
        if direct_parent_loads(data):
            raise ValueError(f"direct parent read found in target: {entry}")
    check_store(target_data["00525c7a"], "00525c7a", 0xA0, ARG)
    check_store(target_data["00525d8e"], "00525d8e", 0xC0, ARG)
    check_store(target_data["00525d4c"], "00525d4c", 0xA8, ARG)
    check_store(target_data["005270f8"], "00527102", 0xCC, "const:0x0:1")
    expected_stores = {"00525c7a": 1, "00525d8e": 1, "00525d4c": 1,
                       "005270f8": 1, "00525fe0": 0, "00526fce": 0}
    for entry, expected in expected_stores.items():
        actual = sum(op["opcode"] == "STORE" for op in target_data[entry])
        if actual != expected:
            raise ValueError(f"target STORE inventory changed: {entry}: {actual}")
    return {"format": FORMAT, "child_static_targets": target_rows,
            "direct_child_writes": [
                {"target": "_setZOrder", "offset": "0xa0", "value": "argument"},
                {"target": "setOrderOfArrival", "offset": "0xc0", "value": "argument"},
                {"target": "setParent", "offset": "0xa8", "value": "argument (null at detach site)"},
                {"target": "onExit", "offset": "0xcc", "value": "0"}],
            "explicit_parent_field_loads_in_targets": 0,
            "recursive_or_indirect_child_calls_closed": False,
            "runtime_dispatch_observed": False, "alias_frontier_closed": False,
            "receiver_promotions": 0, "affects_routing": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("prior_evidence", type=Path)
    parser.add_argument("target_evidence", type=Path)
    parser.add_argument("parent_evidence", type=Path)
    parser.add_argument("catalog", type=Path)
    parser.add_argument("v34_report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.prior_evidence, args.target_evidence, args.parent_evidence,
                   args.catalog, args.v34_report)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("static child targets: 6; direct child writes: 4; parent reads: 0; receiver promotions: 0")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
