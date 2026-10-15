#!/usr/bin/env python3
"""Audit direct parent-field readers in CCNode and CCSprite p-code."""
from __future__ import annotations

import argparse
import csv
import json
from collections import deque
from pathlib import Path

from audit_nevergone_beaten_factory_fields_v26 import definitions, inputs, rows, unwrap

FORMAT = "NEVERGONE.BeatenChildParentReadsV33/1"
EXPECTED = {
    ("00525c80", "00525c8c", "23"),
    ("00525d46", "00525d46", "1"),
    ("00525dde", "00525de2", "12"),
    ("00526e0c", "00526e10", "8"),
    ("00526e0c", "00526e1a", "16"),
    ("0054934a", "00549354", "17"),
    ("005499b4", "005499ce", "42"),
}
TRANSPARENT = {"COPY", "CAST", "INDIRECT", "MULTIEQUAL", "INT_ADD", "PTRADD", "PTRSUB"}


def trace(data: list[dict[str, str]], root: str) -> list[dict[str, str]]:
    tainted = {root}
    pending = deque([root])
    effects: dict[tuple[str, str, str], dict[str, str]] = {}
    by_input: dict[str, list[tuple[dict[str, str], int]]] = {}
    for op in data:
        for index, value in enumerate(inputs(op)):
            by_input.setdefault(value, []).append((op, index))
    while pending:
        value = pending.popleft()
        for op, index in by_input.get(value, []):
            opcode = op["opcode"]
            if opcode == "STORE":
                kind = "direct_store_address" if index == 1 else "store_value" if index == 2 else "none"
            elif opcode == "LOAD":
                kind = "parent_derived_load" if index == 1 else "none"
            elif opcode in {"CALL", "CALLIND"}:
                kind = "call_argument"
            elif opcode == "RETURN":
                kind = "return_value" if index > 0 else "none"
            else:
                kind = "none"
            if kind != "none":
                key = (op["op_address"], op["op_time"], kind)
                effects[key] = {"address": op["op_address"], "time": op["op_time"],
                                "opcode": opcode, "kind": kind,
                                "callee": op["callee_name"] if opcode == "CALL" else "-"}
            if opcode in TRANSPARENT and op["output"] != "-" and op["output"] not in tainted:
                tainted.add(op["output"])
                pending.append(op["output"])
    return sorted(effects.values(), key=lambda item: (item["address"], int(item["time"]), item["kind"]))


def audit(inventory: Path, pcode_dir: Path, previous: Path) -> dict[str, object]:
    v32 = json.loads(previous.read_text(encoding="utf-8"))
    if (v32.get("format") != "NEVERGONE.BeatenInheritedOwnerAuditV32/1"
            or v32.get("child_parent_field_offset") != "0xa8"
            or not v32.get("child_stores_owner_pointer_static_path")):
        raise ValueError("v32 child parent alias not established")
    with inventory.open(encoding="utf-8", newline="") as stream:
        candidate_rows = list(csv.DictReader(stream, delimiter="\t"))
    keys = {(row["function_entry"], row["load_address"], row["load_time"])
            for row in candidate_rows}
    if len(candidate_rows) != len(EXPECTED) or keys != EXPECTED:
        raise ValueError(f"parent load inventory changed: {keys ^ EXPECTED}")
    evidence = []
    for row in candidate_rows:
        entry, address, time = row["function_entry"], row["load_address"], row["load_time"]
        data = rows(pcode_dir / f"pcode-{entry}.tsv")
        if {op["function_entry"] for op in data} != {entry}:
            raise ValueError(f"p-code source changed: {entry}")
        matches = [op for op in data if (op["op_address"], op["op_time"], op["opcode"])
                   == (address, time, "LOAD")]
        if len(matches) != 1 or matches[0]["output"] != row["value_token"]:
            raise ValueError(f"parent load changed: {entry}:{address}:{time}")
        load_parts = inputs(matches[0])
        if len(load_parts) != 2:
            raise ValueError(f"parent load operand shape changed: {entry}:{address}:{time}")
        _, address_op = unwrap(load_parts[1], definitions(data))
        address_parts = inputs(address_op) if address_op is not None else []
        if (address_op is None or address_op["opcode"] not in {"PTRADD", "PTRSUB", "INT_ADD"}
                or len(address_parts) < 2 or address_parts[0] != row["base_token"]
                or address_parts[1] != "const:0xa8:4"
                or (address_op["opcode"] == "PTRADD" and address_parts[2:] != ["const:0x1:4"])):
            raise ValueError(f"parent load address changed: {entry}:{address}:{time}")
        effects = trace(data, row["value_token"])
        evidence.append({"function_entry": entry, "function_name": row["function_name"],
                         "load_address": address, "load_time": time,
                         "base_token": row["base_token"], "effects": effects})
    direct_stores = [effect for item in evidence for effect in item["effects"]
                     if effect["kind"] == "direct_store_address"]
    if direct_stores:
        raise ValueError(f"parent-derived direct STORE destination found: {direct_stores}")
    return {"format": FORMAT, "scope": "direct +0xa8 loads in CCNode/CCSprite named functions",
            "candidate_load_count": len(evidence), "candidate_function_count": len({e["function_entry"] for e in evidence}),
            "direct_parent_derived_store_count": 0,
            "indirect_call_side_effects_closed": False, "getter_callers_closed": False,
            "alias_frontier_closed": False, "receiver_promotions": 0, "affects_routing": False,
            "reads": evidence}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inventory", type=Path)
    parser.add_argument("pcode_dir", type=Path)
    parser.add_argument("v32_report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.inventory, args.pcode_dir, args.v32_report)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("parent loads: 7 in 6 functions; direct parent-derived stores: 0; receiver promotions: 0")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
