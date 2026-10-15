#!/usr/bin/env python3
"""Check v25 BeatenEffect factory stores against the unresolved setOpacity fields.

This is an evidence gate, not a receiver-type promotion. A factory write does
not close the owner alias frontier or prove that every later value is a sprite.
"""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path


FORMAT = "NEVERGONE.BeatenFactoryFieldAuditV26/1"
FACTORY = "cocos2d::CCSprite::createWithSpriteFrame"
SOURCES = {"002a3a20": 0x158, "002a3c74": 0x14C}
SET_OPACITY = "002a47c6"
PASS_THROUGH = {"CAST", "COPY"}


def rows(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as stream:
        return list(csv.DictReader(stream, delimiter="\t"))


def inputs(row: dict[str, str]) -> list[str]:
    return [part for part in row["inputs"].split(";") if part and part != "-"]


def definitions(data: list[dict[str, str]]) -> dict[str, dict[str, str]]:
    return {row["output"]: row for row in data if row["output"].startswith("def:")}


def unwrap(token: str, defs: dict[str, dict[str, str]]) -> tuple[str, dict[str, str] | None]:
    seen: set[str] = set()
    while token in defs and token not in seen:
        seen.add(token)
        row = defs[token]
        if row["opcode"] not in PASS_THROUGH or len(inputs(row)) != 1:
            return token, row
        token = inputs(row)[0]
    return token, defs.get(token)


def field_address(token: str, defs: dict[str, dict[str, str]]) -> tuple[str, int] | None:
    _, row = unwrap(token, defs)
    if row is None or row["opcode"] != "INT_ADD":
        return None
    parts = inputs(row)
    if len(parts) != 2 or not parts[1].startswith("const:"):
        return None
    return parts[0], int(parts[1].split(":")[1], 16)


def owner_path(token: str, defs: dict[str, dict[str, str]]) -> dict[str, object]:
    """Recognize direct this, or a loop phi with a +4 self-edge and this root."""
    if token == "register:register:00000020:4":
        return {"kind": "direct_this", "stride": 0}
    phi_token, phi = unwrap(token, defs)
    if phi is None or phi["opcode"] != "MULTIEQUAL":
        return {"kind": "unknown", "stride": None}
    branches = [unwrap(part, defs)[0] for part in inputs(phi)]
    if "register:register:00000020:4" not in branches:
        return {"kind": "unknown", "stride": None}
    for branch in branches:
        _, step = unwrap(branch, defs)
        if step is None or step["opcode"] != "INT_ADD":
            continue
        parts = inputs(step)
        if len(parts) == 2 and parts[0] == phi_token and parts[1] == "const:0x4:4":
            return {"kind": "loop_this_plus_four", "stride": 4}
    return {"kind": "unknown", "stride": None}


def factory_stores(data: list[dict[str, str]], offset: int) -> list[dict[str, object]]:
    defs = definitions(data)
    result = []
    for row in data:
        if row["opcode"] != "STORE":
            continue
        parts = inputs(row)
        if len(parts) != 3:
            continue
        address = field_address(parts[1], defs)
        if address is None or address[1] != offset:
            continue
        _, value = unwrap(parts[2], defs)
        path = owner_path(address[0], defs)
        result.append({
            "store_address": row["op_address"],
            "field_offset": f"0x{offset:x}",
            "owner_path": path,
            "factory_address": value["op_address"] if value and value["opcode"] == "CALL" else None,
            "factory_name": value["callee_name"] if value and value["opcode"] == "CALL" else None,
        })
    return result


def set_opacity_fields(data: list[dict[str, str]]) -> dict[int, int]:
    fields: dict[int, int] = {}
    for row in data:
        if row.get("row_type") != "node" or row.get("depth") not in {"3", "4"}:
            continue
        if not row.get("node_token", "").startswith("const:0x"):
            continue
        offset = int(row["node_token"].split(":")[1], 16)
        if offset in {0x128, 0x12C, 0x130, 0x134, 0x138, 0x14C, 0x158}:
            fields[int(row["call_index"])] = offset
    return fields


def audit(pcode_dir: Path, slice_path: Path) -> dict[str, object]:
    observed = set_opacity_fields(rows(slice_path))
    expected = {0: 0x130, 1: 0x134, 2: 0x138, 3: 0x12C, 4: 0x128, 5: 0x158, 6: 0x14C}
    if observed != expected:
        raise ValueError(f"setOpacity receiver fields changed: {observed!r}")
    evidence = []
    for entry, offset in SOURCES.items():
        path = pcode_dir / f"pcode-{entry}.tsv"
        data = rows(path)
        if not data or {row["function_entry"] for row in data} != {entry}:
            raise ValueError(f"unexpected source in {path}")
        stores = factory_stores(data, offset)
        matching = [s for s in stores if s["factory_name"] == FACTORY and s["owner_path"]["kind"] != "unknown"]
        if len(matching) != 1:
            raise ValueError(f"expected one proved sprite factory store at {entry}+0x{offset:x}: {stores!r}")
        evidence.append({"source_entry": entry, "set_opacity_calls": [i for i, field in expected.items() if field == offset], **matching[0]})
    return {"format": FORMAT, "accepted_factory_stores": evidence,
            "set_opacity_fields": {str(i): f"0x{field:x}" for i, field in sorted(observed.items())},
            "receiver_promotions": 0, "alias_frontier_closed": False, "affects_routing": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pcode_dir", type=Path)
    parser.add_argument("slice_path", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.pcode_dir, args.slice_path)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(f"accepted factory stores: {len(result['accepted_factory_stores'])}; receiver promotions: 0")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
