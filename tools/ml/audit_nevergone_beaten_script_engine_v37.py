#!/usr/bin/env python3
"""Join BeatenEffect lifecycle script calls with exact-APK engine reachability."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from audit_nevergone_beaten_factory_fields_v26 import definitions, inputs, rows, unwrap
from audit_nevergone_controls_parent_script_static_closure import analyze as analyze_apk

FORMAT = "NEVERGONE.BeatenScriptEngineV37/1"
ROOT = "register:register:00000020:4"
METHODS = {
    "005270f8": ("00527106", "0052710a", "INT_NOTEQUAL", "const:0x0:4",
                 "0052710c", "00527114", "00527116", "00527118", "0052711a"),
    "00525fe0": ("0052600c", "00526010", "INT_EQUAL", "const:0x2:4",
                 "00526014", "0052601c", "0052601e", "00526020", "00526022"),
    "00526fce": ("00526fdc", "00526fe0", "INT_NOTEQUAL", "const:0x0:4",
                 "00526fe2", "00526fea", "00526fec", "00526fee", "00526ff0"),
}


def one(data: list[dict[str, str]], address: str, opcode: str) -> dict[str, str]:
    found = [op for op in data if op["op_address"] == address and op["opcode"] == opcode]
    if len(found) != 1:
        raise ValueError(f"expected one {opcode} at {address}, got {len(found)}")
    return found[0]


def loaded_from(data: list[dict[str, str]], load: dict[str, str], source: dict[str, str]) -> bool:
    parts = inputs(load)
    return len(parts) == 2 and unwrap(parts[1], definitions(data))[1] == source


def audit(apk: Path, evidence: Path, previous: Path) -> dict[str, object]:
    v36 = json.loads(previous.read_text(encoding="utf-8"))
    if (v36.get("format") != "NEVERGONE.BeatenChildLifecycleV36/1"
            or v36.get("script_engine_indirect_call_sites") != 3):
        raise ValueError("v36 script-engine call inventory changed")
    controls = analyze_apk(apk)
    if (controls.get("format") != "NEVERGONE.ControlsParentScriptStaticClosure/1"
            or controls.get("elf_sha256") != "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"):
        raise ValueError("exact APK script-engine closure changed")
    channel = controls["script_engine_channel"]
    if (channel.get("manager_engine_initial_value") != "null"
            or channel.get("packaged_static_script_engine_assignment_found") is not False
            or channel.get("dynamic_symbol_lookup_or_runtime_injection_excluded") is not False):
        raise ValueError("script-engine reachability changed")
    call_sites = []
    for entry, (guard_addr, condition_addr, condition_op, comparison,
                manager_addr, engine_addr, vtable_addr, slot_addr, call_addr) in METHODS.items():
        data = rows(evidence / f"pcode-{entry}.tsv")
        if {op["function_entry"] for op in data} != {entry}:
            raise ValueError(f"lifecycle function changed: {entry}")
        guard_ptr = one(data, guard_addr, "PTRADD")
        guard_load = one(data, guard_addr, "LOAD")
        if (inputs(guard_ptr) != [ROOT, "const:0xdc:4", "const:0x1:4"]
                or not loaded_from(data, guard_load, guard_ptr)):
            raise ValueError(f"script handler guard changed: {entry}")
        condition = one(data, condition_addr, condition_op)
        if inputs(condition) != [guard_load["output"], comparison]:
            raise ValueError(f"script handler condition changed: {entry}")
        manager = one(data, manager_addr, "CALL")
        engine = one(data, engine_addr, "LOAD")
        vtable = one(data, vtable_addr, "LOAD")
        slot = one(data, slot_addr, "INT_ADD")
        slot_load = one(data, slot_addr, "LOAD")
        indirect = one(data, call_addr, "CALLIND")
        if (manager["callee_name"] != "cocos2d::CCScriptEngineManager::sharedManager"
                or not loaded_from(data, engine, manager)
                or not loaded_from(data, vtable, engine)
                or inputs(slot) != [vtable["output"], "const:0x20:4"]
                or not loaded_from(data, slot_load, slot)):
            raise ValueError(f"script engine vtable path changed: {entry}")
        arguments = inputs(indirect)
        if entry == "00525fe0" and arguments != [engine["output"], ROOT, "const:0x3:4"]:
            # The engine pointer may be wrapped in a transparent CAST.
            first = arguments[0] if arguments else "-"
            if (len(arguments) != 3 or unwrap(first, definitions(data))[1] != engine
                    or arguments[1:] != [ROOT, "const:0x3:4"]):
                raise ValueError("transition callback arguments changed")
        call_sites.append({"function_entry": entry, "script_handler_guard_offset": "0xdc",
                           "engine_vtable_slot": "0x20", "indirect_call": call_addr,
                           "exported_arguments": arguments,
                           "arguments_complete": bool(arguments)})
    return {"format": FORMAT, "elf_sha256": controls["elf_sha256"],
            "script_engine_calls": call_sites,
            "manager_engine_initial_value": "null",
            "packaged_static_engine_assignment_found": False,
            "dynamic_symbol_lookup_or_runtime_injection_excluded": False,
            "engine_call_runtime_reachability_proven": False,
            "script_side_effects_closed": False,
            "receiver_promotions": 0, "affects_routing": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original_apk", type=Path)
    parser.add_argument("evidence", type=Path)
    parser.add_argument("v36_report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.original_apk, args.evidence, args.v36_report)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("script-engine calls: 3; packaged static assignment: none; runtime injection: open; promotions: 0")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
