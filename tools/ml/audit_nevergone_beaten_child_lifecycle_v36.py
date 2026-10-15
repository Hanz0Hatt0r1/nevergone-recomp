#!/usr/bin/env python3
"""Audit the child-array recursion and script callback boundary in lifecycle methods."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from audit_nevergone_beaten_factory_fields_v26 import definitions, inputs, rows, unwrap

FORMAT = "NEVERGONE.BeatenChildLifecycleV36/1"
ROOT = "register:register:00000020:4"
METHODS = {
    "005270f8": {"name": "cocos2d::CCNode::onExit", "slot": "0x138",
                 "array": "0052711c", "count": "00527122", "data_array": "0052712a", "data": "00527130",
                 "slot_op": "00527152", "child_call": "00527156", "script_call": "0052711a",
                 "manager_call": "0052710c", "script_slot": "00527118"},
    "00525fe0": {"name": "cocos2d::CCNode::onExitTransitionDidStart", "slot": "0x13c",
                 "array": "00525fe4", "count": "00525fea", "data_array": "00525ff0", "data": "00525ff6",
                 "slot_op": "00526030", "child_call": "00526034", "script_call": "00526022",
                 "manager_call": "00526014", "script_slot": "00526020"},
    "00526fce": {"name": "cocos2d::CCNode::cleanup", "slot": "0x140",
                 "array": "00526ff2", "count": "00526ff8", "data_array": "00527000", "data": "00527006",
                 "slot_op": "00527028", "child_call": "0052702c", "script_call": "00526ff0",
                 "manager_call": "00526fe2", "script_slot": "00526fee"},
}


def one(data: list[dict[str, str]], address: str, opcode: str) -> dict[str, str]:
    matches = [op for op in data if op["op_address"] == address and op["opcode"] == opcode]
    if len(matches) != 1:
        raise ValueError(f"expected one {opcode} at {address}, got {len(matches)}")
    return matches[0]


def audit(evidence: Path, v35_report: Path) -> dict[str, object]:
    previous = json.loads(v35_report.read_text(encoding="utf-8"))
    if (previous.get("format") != "NEVERGONE.BeatenChildDispatchV35/1"
            or previous.get("explicit_parent_field_loads_in_targets") != 0):
        raise ValueError("v35 child target frontier changed")
    prior_slots = {target["slot"] for target in previous.get("child_static_targets", [])}
    if not {"0x138", "0x13c", "0x140"}.issubset(prior_slots):
        raise ValueError("v35 lifecycle slot candidates missing")
    results = []
    for entry, spec in METHODS.items():
        data = rows(evidence / f"pcode-{entry}.tsv")
        if {op["function_entry"] for op in data} != {entry}:
            raise ValueError(f"lifecycle p-code source changed: {entry}")
        defs = definitions(data)
        array_address = one(data, spec["array"], "PTRADD")
        if inputs(array_address) != [ROOT, "const:0xa4:4", "const:0x1:4"]:
            raise ValueError(f"child array field changed in {entry}")
        array_load = one(data, spec["array"], "LOAD")
        _, load_address = unwrap(inputs(array_load)[1], defs)
        if load_address != array_address:
            raise ValueError(f"child array LOAD changed in {entry}")
        count = one(data, spec["count"], "CALL")
        if count["callee_name"] != "cocos2d::CCArray::count" or inputs(count) != [array_load["output"]]:
            raise ValueError(f"child array count changed in {entry}")
        second_array_address = one(data, spec["data_array"], "PTRADD")
        second_array_load = one(data, spec["data_array"], "LOAD")
        if (inputs(second_array_address) != [ROOT, "const:0xa4:4", "const:0x1:4"]
                or unwrap(inputs(second_array_load)[1], defs)[1] != second_array_address):
            raise ValueError(f"child array data source changed in {entry}")
        data_access = one(data, spec["data"], "INT_ADD")
        if inputs(data_access) != [second_array_load["output"], "const:0x14:4"]:
            raise ValueError(f"child array data access changed in {entry}")
        slot = one(data, spec["slot_op"], "INT_ADD")
        if inputs(slot)[-1] != f"const:{spec['slot']}:4":
            raise ValueError(f"recursive child slot changed in {entry}")
        child_call = one(data, spec["child_call"], "CALLIND")
        script_call = one(data, spec["script_call"], "CALLIND")
        manager = one(data, spec["manager_call"], "CALL")
        script_slot = one(data, spec["script_slot"], "INT_ADD")
        if (manager["callee_name"] != "cocos2d::CCScriptEngineManager::sharedManager"
                or inputs(script_slot)[-1] != "const:0x20:4"):
            raise ValueError(f"script callback path changed in {entry}")
        if child_call == script_call or sum(op["opcode"] == "CALLIND" for op in data) != 2:
            raise ValueError(f"lifecycle indirect call inventory changed in {entry}")
        if any("const:0xa8:4" in inputs(op) for op in data):
            raise ValueError(f"parent field offset surfaced in lifecycle method {entry}")
        results.append({"entry": entry, "name": spec["name"], "child_array_offset": "0xa4",
                        "recursive_child_slot": spec["slot"],
                        "recursive_child_call": spec["child_call"],
                        "script_engine_indirect_call": spec["script_call"],
                        "script_engine_call_arguments": inputs(script_call),
                        "child_call_arguments": inputs(child_call)})
    return {"format": FORMAT, "lifecycle_methods": results,
            "static_child_array_recursion_count": len(results),
            "script_engine_indirect_call_sites": len(results),
            "explicit_parent_offset_uses_in_bodies": 0,
            "script_engine_call_side_effects_closed": False,
            "recursive_descendant_dispatch_closed": False,
            "runtime_dispatch_observed": False, "alias_frontier_closed": False,
            "receiver_promotions": 0, "affects_routing": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("v35_report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.evidence, args.v35_report)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("lifecycle child recursions: 3; script-engine indirect calls: 3; parent offset uses: 0; promotions: 0")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
