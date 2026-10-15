#!/usr/bin/env python3
"""Audit owner-rooted stores and explicit pointer escapes in two callbacks.

The v29/v30 chain establishes their callback target and owner argument. This
audit treats r0 as that owner only for these two exact functions. A LOAD from
an owner field is a value, not an escape of the owner address.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from audit_nevergone_beaten_factory_fields_v26 import definitions, field_address, inputs, owner_path, rows, unwrap

FORMAT = "NEVERGONE.BeatenCallbackOwnerFlowV31/1"
ROOT = "register:register:00000020:4"
TRANSPARENT = {"CAST", "COPY", "INDIRECT", "INT_ADD", "PTRADD", "PTRSUB", "MULTIEQUAL", "PIECE", "SUBPIECE"}
EXPECTED = {
    "002a3a20": {
        "002a3aca": ("0x158", "direct_this", "cocos2d::CCSprite::createWithSpriteFrame"),
        "002a3c28": ("0x148", "direct_this", "GameSceneEffectObject::createWithObject"),
        "002a3c3c": ("0x148", "direct_this", "null"),
    },
    "002a3c74": {
        "002a3ce6": ("0x14c", "loop_this_plus_four", "cocos2d::CCSprite::createWithSpriteFrame"),
        "002a3de4": ("0x13c", "loop_this_plus_four", "GameSceneEffectObject::createWithObject"),
    },
}


def owner_origin(token: str, defs: dict[str, dict[str, str]]) -> bool:
    todo = [token]
    seen: set[str] = set()
    while todo:
        current = todo.pop()
        if current == ROOT:
            return True
        if current in seen:
            continue
        seen.add(current)
        if len(seen) > 10000:
            raise ValueError("owner-origin p-code budget exceeded")
        row = defs.get(current)
        if row is not None and row["opcode"] in TRANSPARENT:
            todo.extend(inputs(row))
    return False


def prior_evidence(alias_path: Path, selector_path: Path, loop_path: Path) -> None:
    alias = json.loads(alias_path.read_text(encoding="utf-8"))
    selector = json.loads(selector_path.read_text(encoding="utf-8"))
    loop = json.loads(loop_path.read_text(encoding="utf-8"))
    if (alias.get("format") != "NEVERGONE.BeatenCallbackAliasAuditV29/1"
            or alias.get("retained_owner_alias_static_path_proven") is not True
            or alias.get("callback_owner_argument_forwarded") is not True):
        raise ValueError("v29 owner callback chain not established")
    if (selector.get("format") != "NEVERGONE.BeatenCallbackSelectorAuditV30/1"
            or {r.get("target_entry") for r in selector.get("resolved_callback_selectors", [])} != set(EXPECTED)):
        raise ValueError("v30 callback target set changed")
    if (loop.get("format") != "NEVERGONE.BeatenLoopWriterAuditV27/1"
            or loop.get("factory_fields_in_three_iteration_loop") != ["0x14c", "0x150", "0x154"]):
        raise ValueError("v27 loop bound evidence changed")


def audit(pcode_dir: Path, alias_path: Path, selector_path: Path, loop_path: Path) -> dict[str, object]:
    prior_evidence(alias_path, selector_path, loop_path)
    functions = []
    for entry, expected in EXPECTED.items():
        data = rows(pcode_dir / f"pcode-{entry}.tsv")
        if not data or {r["function_entry"] for r in data} != {entry}:
            raise ValueError(f"p-code source changed: {entry}")
        defs = definitions(data)
        stores = [r for r in data if r["opcode"] == "STORE"]
        if {r["op_address"] for r in stores} != set(expected) or len(stores) != len(expected):
            raise ValueError(f"callback STORE frontier changed: {entry}")
        checked = []
        for store in stores:
            parts = inputs(store)
            if len(parts) != 3:
                raise ValueError(f"STORE arity changed: {store['op_address']}")
            address = field_address(parts[1], defs)
            if address is None:
                raise ValueError(f"unresolved callback STORE address: {store['op_address']}")
            path = owner_path(address[0], defs)
            value, source = unwrap(parts[2], defs)
            source_name = "null" if value == "const:0x0:4" else (
                source["callee_name"] if source is not None and source["opcode"] == "CALL" else "unknown"
            )
            actual = (f"0x{address[1]:x}", path["kind"], source_name)
            if actual != expected[store["op_address"]]:
                raise ValueError(f"callback STORE provenance changed: {store['op_address']}: {actual!r}")
            checked.append({"store_address": store["op_address"], "base_field": actual[0],
                            "owner_path": actual[1], "loop_stride": path["stride"], "value_source": actual[2]})
        escapes = []
        for row in data:
            if row["opcode"] in {"CALL", "CALLIND", "RETURN"}:
                for arg_index, token in enumerate(inputs(row)):
                    if owner_origin(token, defs):
                        escapes.append({"op_address": row["op_address"], "opcode": row["opcode"],
                                        "argument": arg_index, "token": token})
            elif row["opcode"] == "STORE":
                parts = inputs(row)
                if len(parts) == 3 and owner_origin(parts[2], defs):
                    escapes.append({"op_address": row["op_address"], "opcode": "STORE_VALUE",
                                    "argument": 2, "token": parts[2]})
        if escapes:
            raise ValueError(f"callback owner-address escape found: {entry}: {escapes!r}")
        functions.append({"function_entry": entry, "owner_root": ROOT, "stores": checked,
                          "explicit_owner_address_escapes": 0,
                          "calls_inspected": sum(r["opcode"] in {"CALL", "CALLIND"} for r in data)})
    return {"format": FORMAT, "callback_functions": functions,
            "owner_root_scope": "two exact v30 callback targets only",
            "alias_frontier_closed": False, "receiver_promotions": 0, "affects_routing": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcode_dir", type=Path)
    parser.add_argument("v29_alias_report", type=Path)
    parser.add_argument("v30_selector_report", type=Path)
    parser.add_argument("v27_loop_report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.pcode_dir, args.v29_alias_report, args.v30_selector_report, args.v27_loop_report)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("callback owner stores: 5; explicit owner-address escapes: 0; receiver promotions: 0")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
