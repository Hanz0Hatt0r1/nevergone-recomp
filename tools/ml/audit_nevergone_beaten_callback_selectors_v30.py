#!/usr/bin/env python3
"""Resolve two BeatenEffect callback selectors through PIC literals and GOT cells.

The Ghidra p-code and memory exports are checked together. Resolution of a
selector does not by itself close its callback's field-write frontier.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from audit_nevergone_beaten_factory_fields_v26 import definitions, inputs, rows, unwrap

FORMAT = "NEVERGONE.BeatenCallbackSelectorAuditV30/1"
CASES = [
    {"call": "002a3f82", "load": "002a3f74", "add": "002a3f72", "literal_instruction": "002a3f6c",
     "literal": "002a4178", "pc": 0x2A3F76, "got": 0x93EDE4, "target": 0x2A3A20,
     "name": "BeatenEffect::createBeatenBloodPointEffect02"},
    {"call": "002a40e2", "load": "002a40ce", "add": "002a40cc", "literal_instruction": "002a40ca",
     "literal": "002a4180", "pc": 0x2A40D0, "got": 0x93EDE8, "target": 0x2A3C74,
     "name": "BeatenEffect::createBeatenBloodPointEffect"},
]


def index(path: Path) -> dict[str, dict[str, str]]:
    data = rows(path)
    result = {r["address"]: r for r in data}
    if len(result) != len(data):
        raise ValueError(f"duplicate address classification in {path}")
    return result


def reaches_address(token: str, defs: dict[str, dict[str, str]], address: str) -> bool:
    todo = [token]
    seen = set()
    goal = f"address:{address}:4"
    while todo:
        current = todo.pop()
        if current == goal:
            return True
        if current in seen:
            continue
        seen.add(current)
        if len(seen) > 10000:
            raise ValueError("p-code ancestry budget exceeded")
        row = defs.get(current)
        if row is not None:
            todo.extend(inputs(row))
    return False


def audit(operands_path: Path, pcode_path: Path, literals_path: Path,
          cells_path: Path, targets_path: Path) -> dict[str, object]:
    operands = rows(operands_path)
    pcode = rows(pcode_path)
    defs = definitions(pcode)
    literals, cells, targets = index(literals_path), index(cells_path), index(targets_path)
    result = []
    for case in CASES:
        literal = case["literal"]
        operand = [r for r in operands if r["instruction"] == case["literal_instruction"]
                   and r["kind"] == "referenced-scalar" and r["target"] == literal]
        if len(operand) != 1:
            raise ValueError(f"callback literal reference changed at {case['literal_instruction']}")
        lit = literals.get(literal)
        if lit is None or lit["raw_u32"] != operand[0]["unsigned_hex"]:
            raise ValueError(f"callback literal value mismatch at {literal}")
        raw_offset = int(lit["raw_u32"], 16)
        got_address = case["pc"] + raw_offset
        if got_address != case["got"]:
            raise ValueError(f"callback GOT address changed: {got_address:#x}")
        add = [r for r in pcode if r["op_address"] == case["add"] and r["opcode"] == "INT_ADD"
               and r["output"].startswith(f"def:{case['add']}:")]
        if len(add) != 1 or inputs(add[0])[-1] != f"const:0x{case['pc']:x}:4":
            raise ValueError(f"callback PIC add changed at {case['add']}")
        if not reaches_address(inputs(add[0])[0], defs, literal):
            raise ValueError(f"callback PIC base no longer traces to {literal}")
        load = [r for r in pcode if r["op_address"] == case["load"] and r["opcode"] == "LOAD"]
        if len(load) != 1 or len(inputs(load[0])) != 2:
            raise ValueError(f"callback selector LOAD changed at {case['load']}")
        _, load_address = unwrap(inputs(load[0])[1], defs)
        if load_address != add[0]:
            raise ValueError(f"callback selector LOAD address changed at {case['load']}")
        cell = cells.get(f"{case['got']:08x}")
        if cell is None or cell["block"] != ".got" or cell["data_type"] != "pointer":
            raise ValueError(f"callback GOT cell classification changed at {case['got']:#x}")
        pointer = int(cell["raw_u32"], 16)
        if pointer & 1 != 1 or pointer & ~1 != case["target"]:
            raise ValueError(f"callback Thumb pointer changed at {case['got']:#x}")
        target = targets.get(f"{case['target']:08x}")
        if (target is None or target["function_at_entry"] != f"{case['target']:08x}"
                or target["function_at_name"] != case["name"] or target["execute"] != "1"):
            raise ValueError(f"callback target function changed at {case['target']:#x}")
        calls = [r for r in pcode if r["op_address"] == case["call"] and r["opcode"] == "CALL"]
        if len(calls) != 1 or calls[0]["callee_name"] != "cocos2d::CCCallFunc::create" or load[0]["output"] not in inputs(calls[0]):
            raise ValueError(f"callback selector not passed to CCCallFunc::create at {case['call']}")
        result.append({"call_address": case["call"], "literal_address": literal,
                       "literal_offset": lit["raw_u32"], "pc_base": f"0x{case['pc']:x}",
                       "got_cell": f"0x{case['got']:x}", "raw_thumb_pointer": cell["raw_u32"],
                       "target_entry": f"{case['target']:08x}", "target_name": case["name"]})
    return {"format": FORMAT, "resolved_callback_selectors": result,
            "callback_targets_identified": 2, "alias_frontier_closed": False,
            "receiver_promotions": 0, "affects_routing": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("operands", type=Path)
    parser.add_argument("pcode", type=Path)
    parser.add_argument("literals", type=Path)
    parser.add_argument("got_cells", type=Path)
    parser.add_argument("targets", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.operands, args.pcode, args.literals, args.got_cells, args.targets)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("callback selectors resolved: 2; receiver promotions: 0")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
