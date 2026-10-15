#!/usr/bin/env python3
"""Verify the three-element BeatenEffect sprite and destructor loops.

The report narrows two v23 owner-derived unknown stores to null writes in the
destructor. It does not claim that aliases outside the owner are closed.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from audit_nevergone_beaten_factory_fields_v26 import audit as audit_v26, definitions, inputs, rows, unwrap

FORMAT = "NEVERGONE.BeatenLoopWriterAuditV27/1"


def require(defs: dict[str, dict[str, str]], token: str, opcode: str, expected: list[str]) -> None:
    row = defs.get(token)
    if row is None or row["opcode"] != opcode or inputs(row) != expected:
        raise ValueError(f"unexpected {token}: {row!r}; expected {opcode} {expected!r}")


def prove_factory_loop(data: list[dict[str, str]]) -> None:
    d = definitions(data)
    require(d, "def:002a3c88:584:4", "MULTIEQUAL", ["def:002a3c86:1322:4", "def:002a3df8:1323:4"])
    require(d, "def:002a3c86:1322:4", "COPY", ["register:register:00000020:4"])
    require(d, "def:002a3df8:1323:4", "COPY", ["def:002a3cc2:97:4"])
    require(d, "def:002a3cc2:97:4", "INT_ADD", ["def:002a3c88:584:4", "const:0x4:4"])
    require(d, "def:002a3c88:587:4", "MULTIEQUAL", ["def:002a3c86:28:4", "def:002a3df6:347:4"])
    require(d, "def:002a3c86:28:4", "COPY", ["const:0x3:4"])
    require(d, "def:002a3df6:347:4", "INT_ADD", ["def:002a3c88:587:4", "const:0xffffffff:4"])
    require(d, "def:002a3df6:349:1", "INT_NOTEQUAL", ["def:002a3df6:347:4", "const:0x0:4"])
    branch = [r for r in data if r["op_address"] == "002a3df8" and r["opcode"] == "CBRANCH"]
    if len(branch) != 1 or inputs(branch[0]) != ["address:002a3c88:1", "def:002a3df6:349:1"]:
        raise ValueError("factory loop backedge changed")


def prove_destructor_loop(data: list[dict[str, str]]) -> list[dict[str, str]]:
    d = definitions(data)
    require(d, "def:002a393c:341:4", "MULTIEQUAL", ["def:002a3938:635:4", "def:002a3976:636:4"])
    require(d, "def:002a3938:635:4", "COPY", ["register:register:00000020:4"])
    require(d, "def:002a3976:636:4", "COPY", ["def:002a3966:114:4"])
    require(d, "def:002a3966:114:4", "PTRADD", ["def:002a393c:341:4", "const:0x4:4", "const:0x1:4"])
    require(d, "def:002a393c:342:4", "MULTIEQUAL", ["def:002a390c:14:4", "def:002a3964:105:4"])
    require(d, "def:002a390c:14:4", "COPY", ["const:0x0:4"])
    require(d, "def:002a3964:105:4", "INT_ADD", ["def:002a393c:342:4", "const:0x1:4"])
    require(d, "def:002a3970:135:1", "INT_NOTEQUAL", ["def:002a3964:105:4", "const:0x3:4"])
    branch = [r for r in data if r["op_address"] == "002a3976" and r["opcode"] == "CBRANCH"]
    if len(branch) != 1 or inputs(branch[0]) != ["address:002a393c:1", "def:002a3970:135:1"]:
        raise ValueError("destructor loop backedge changed")
    result = []
    for address, offset in [("002a3956", 0x13C), ("002a3972", 0x14C)]:
        stores = [r for r in data if r["op_address"] == address and r["opcode"] == "STORE"]
        if len(stores) != 1 or inputs(stores[0]) != ["const:0x1a1:4", f"def:{address}:{646 if offset == 0x13C else 650}:4", "const:0x0:4"]:
            raise ValueError(f"destructor null store changed at {address}")
        _, cast = unwrap(inputs(stores[0])[1], d)
        if cast is None or cast["opcode"] != "PTRADD" or inputs(cast) != ["def:002a393c:341:4", f"const:0x{offset:x}:4", "const:0x1:4"]:
            raise ValueError(f"destructor field address changed at {address}")
        result.append({"store_address": address, "base_field": f"0x{offset:x}", "value": "null"})
    return result


def audit(pcode_dir: Path, destructor: Path, slice_path: Path, owner_flow: Path) -> dict[str, object]:
    previous = audit_v26(pcode_dir, slice_path)
    prove_factory_loop(rows(pcode_dir / "pcode-002a3c74.tsv"))
    null_stores = prove_destructor_loop(rows(destructor))
    unknown = [r for r in rows(owner_flow) if r["row_type"] == "store" and r["address_flow_kind"] == "owner_derived_unknown"]
    if {(r["function_entry"], r["op_address"], r["value_token"]) for r in unknown} != {
        ("002a3908", "002a3956", "const:0x0:4"),
        ("002a3908", "002a3972", "const:0x0:4"),
    }:
        raise ValueError("owner-flow unknown store frontier changed")
    return {"format": FORMAT, "factory_fields_in_three_iteration_loop": ["0x14c", "0x150", "0x154"],
            "factory_store": next(s for s in previous["accepted_factory_stores"] if s["field_offset"] == "0x14c"),
            "destructor_null_store_sites": null_stores,
            "destructor_null_fields_in_three_iteration_loop": ["0x14c", "0x150", "0x154"],
            "unknown_destructor_store_sites_explained": len(unknown),
            "alias_frontier_closed": False, "receiver_promotions": 0, "affects_routing": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcode_dir", type=Path)
    parser.add_argument("destructor_pcode", type=Path)
    parser.add_argument("set_opacity_slice", type=Path)
    parser.add_argument("owner_flow", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = audit(args.pcode_dir, args.destructor_pcode, args.set_opacity_slice, args.owner_flow)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("three factory fields and three destructor null fields verified; receiver promotions: 0")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
