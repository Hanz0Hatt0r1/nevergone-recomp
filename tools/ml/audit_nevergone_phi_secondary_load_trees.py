#!/usr/bin/env python3
"""Inspect and constant-fold the unknown second branch of structural receiver PHIs.

Evidence-only. Reads persistent NEVERGONE.GhidraStructuralReceiverSlice/1 TSV files
and address-classification evidence. Literal-pool DAT_* cells in .text are folded
using their raw_u32 contents rather than their storage addresses. It does not infer
a receiver class or emit routing queries.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any

SLICE_FORMAT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
ADDRESS_FORMAT = "NEVERGONE.GhidraAddressClassification/1"
AUDIT_FORMAT = "NEVERGONE.PhiSecondaryLoadTreeAudit/2"
MASK32 = 0xFFFFFFFF


def parse_int(value: Any, default: int | None = None) -> int | None:
    if value in (None, "", "-"):
        return default
    text = str(value).strip()
    try:
        if re.fullmatch(r"[0-9a-fA-F]+", text) and not text.lower().startswith("0x"):
            return int(text, 16)
        return int(text, 0)
    except ValueError:
        return default


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def address_from_token(token: str | None) -> int | None:
    if not token:
        return None
    match = re.fullmatch(r"address:([0-9a-fA-F]+):\d+", token)
    return int(match.group(1), 16) if match else None


def is_literal_pool_row(row: dict[str, str]) -> bool:
    symbol = row.get("primary_symbol") or ""
    mnemonic = row.get("instruction_mnemonic") or "-"
    data_type = row.get("data_type") or "-"
    return (
        row.get("block") == ".text"
        and symbol.startswith("DAT_")
        and mnemonic == "-"
        and data_type != "-"
    )


def node_children(nodes: dict[int, dict[str, str]]) -> dict[int, list[int]]:
    children: dict[int, list[int]] = defaultdict(list)
    for node_id, row in nodes.items():
        parent = parse_int(row.get("parent_node_id"), -1)
        if parent is not None and parent >= 0:
            children[parent].append(node_id)
    for parent in children:
        children[parent].sort(key=lambda nid: (parse_int(nodes[nid].get("input_index"), -1) or -1, nid))
    return children


def fold_node(
    node_id: int,
    nodes: dict[int, dict[str, str]],
    children: dict[int, list[int]],
    memory_u32: dict[int, int],
    literal_pool_u32: dict[int, int],
    memo: dict[int, int | None],
    unresolved_loads: set[int],
    literal_substitutions: set[int],
    active: set[int],
) -> int | None:
    if node_id in memo:
        return memo[node_id]
    if node_id in active:
        memo[node_id] = None
        return None
    active.add(node_id)

    row = nodes[node_id]
    opcode = row.get("defining_opcode") or "-"
    constant = parse_int(row.get("constant_value"))
    token_address = address_from_token(row.get("node_token"))
    child_ids = children.get(node_id, [])

    def child(nid: int) -> int | None:
        return fold_node(
            nid, nodes, children, memory_u32, literal_pool_u32, memo,
            unresolved_loads, literal_substitutions, active,
        )

    value: int | None = None
    if constant is not None:
        value = constant & MASK32
    elif opcode == "-" and token_address is not None:
        if token_address in literal_pool_u32:
            value = literal_pool_u32[token_address] & MASK32
            literal_substitutions.add(token_address)
        else:
            value = token_address & MASK32
    elif opcode in {"COPY", "CAST", "INDIRECT"} and child_ids:
        value = child(child_ids[0])
    elif opcode == "INT_ADD" and len(child_ids) >= 2:
        a, b = child(child_ids[0]), child(child_ids[1])
        if a is not None and b is not None:
            value = (a + b) & MASK32
    elif opcode == "INT_SUB" and len(child_ids) >= 2:
        a, b = child(child_ids[0]), child(child_ids[1])
        if a is not None and b is not None:
            value = (a - b) & MASK32
    elif opcode == "PTRADD" and len(child_ids) >= 3:
        base, index, scale = child(child_ids[0]), child(child_ids[1]), child(child_ids[2])
        if base is not None and index is not None and scale is not None:
            value = (base + index * scale) & MASK32
    elif opcode == "PTRSUB" and len(child_ids) >= 2:
        base, offset = child(child_ids[0]), child(child_ids[1])
        if base is not None and offset is not None:
            value = (base + offset) & MASK32
    elif opcode == "LOAD" and child_ids:
        address = child(child_ids[0])
        if address is not None:
            if address in memory_u32:
                value = memory_u32[address] & MASK32
            else:
                unresolved_loads.add(address)
    elif opcode == "MULTIEQUAL" and child_ids:
        values = [child(nid) for nid in child_ids]
        known = [x for x in values if x is not None]
        if len(known) == len(values) and known and len(set(known)) == 1:
            value = known[0]

    active.remove(node_id)
    memo[node_id] = value
    return value


def fmt_value(value: int | None) -> str:
    return "-" if value is None else f"0x{value:08x}"


def collect_subtree(root_id: int, children: dict[int, list[int]]) -> list[int]:
    out: list[int] = []
    stack = [root_id]
    while stack:
        node_id = stack.pop()
        out.append(node_id)
        stack.extend(reversed(children.get(node_id, [])))
    return out


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("slice_dir", type=Path)
    parser.add_argument("address_classification_tsv", type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    memory_u32: dict[int, int] = {}
    literal_pool_u32: dict[int, int] = {}
    for line_no, row in enumerate(read_tsv(args.address_classification_tsv), 2):
        if row.get("format") != ADDRESS_FORMAT:
            raise ValueError(f"{args.address_classification_tsv}:{line_no}: expected {ADDRESS_FORMAT}")
        address = parse_int(row.get("address"))
        raw = parse_int(row.get("raw_u32"))
        if address is None or raw is None:
            continue
        memory_u32[address] = raw & MASK32
        if is_literal_pool_row(row):
            literal_pool_u32[address] = raw & MASK32

    grouped: dict[tuple[str, int], dict[int, dict[str, str]]] = defaultdict(dict)
    for path in sorted(args.slice_dir.glob("*.tsv")):
        for line_no, row in enumerate(read_tsv(path), 2):
            if row.get("format") != SLICE_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {SLICE_FORMAT}")
            if row.get("row_type") != "node":
                continue
            entry = row.get("function_entry") or ""
            call_index = parse_int(row.get("call_index"), -1)
            node_id = parse_int(row.get("node_id"), -1)
            if entry and call_index is not None and call_index >= 0 and node_id is not None and node_id >= 0:
                grouped[(entry, call_index)][node_id] = row

    details: list[dict[str, Any]] = []
    selected = 0
    folded = 0
    unresolved_addresses: set[int] = set()
    all_literal_substitutions: set[int] = set()

    print("format:", AUDIT_FORMAT)
    print("literal pool cells available:", [f"0x{x:08x}" for x in sorted(literal_pool_u32)])

    for (entry, call_index), nodes in sorted(grouped.items()):
        root = nodes.get(0)
        if not root or root.get("defining_opcode") != "MULTIEQUAL":
            continue
        children = node_children(nodes)
        branches = children.get(0, [])
        secondary = next((nid for nid in branches if parse_int(nodes[nid].get("input_index"), -1) == 1), None)
        if secondary is None:
            continue

        selected += 1
        memo: dict[int, int | None] = {}
        unresolved: set[int] = set()
        substitutions: set[int] = set()
        value = fold_node(
            secondary, nodes, children, memory_u32, literal_pool_u32,
            memo, unresolved, substitutions, set(),
        )
        if value is not None:
            folded += 1
        unresolved_addresses.update(unresolved)
        all_literal_substitutions.update(substitutions)
        call_address = root.get("call_address")
        slot = root.get("slot_offset")

        print(f"\ntarget: {entry} call {call_index} call_address= {call_address} slot= {slot}")
        print("secondary branch:", secondary, "opcode=", nodes[secondary].get("defining_opcode"), "folded=", fmt_value(value))
        print("literal pool substitutions:", [f"0x{x:08x}" for x in sorted(substitutions)])
        print("unresolved LOAD addresses:", [f"0x{x:08x}" for x in sorted(unresolved)])
        print("tree:")

        subtree = collect_subtree(secondary, children)
        secondary_depth = parse_int(nodes[secondary].get("depth"), 0) or 0
        for node_id in subtree:
            row = nodes[node_id]
            depth = parse_int(row.get("depth"), 0) or 0
            node_value = fold_node(
                node_id, nodes, children, memory_u32, literal_pool_u32,
                memo, unresolved, substitutions, set(),
            )
            indent = "  " * max(0, depth - secondary_depth)
            print(
                f"{indent}node={node_id} input={row.get('input_index')} op={row.get('defining_opcode')} "
                f"type={row.get('type_display')} token={row.get('node_token')} const={row.get('constant_value')} "
                f"folded={fmt_value(node_value)}"
            )

        details.append({
            "format": AUDIT_FORMAT,
            "function_entry": entry,
            "call_index": call_index,
            "call_address": call_address,
            "slot_offset": slot,
            "secondary_node_id": secondary,
            "secondary_opcode": nodes[secondary].get("defining_opcode"),
            "folded_value": value,
            "literal_pool_substitutions": sorted(substitutions),
            "unresolved_load_addresses": sorted(unresolved),
            "affects_routing": False,
        })

    print("\nsummary:")
    print("selected PHI rows:", selected)
    print("fully folded secondary branches:", folded)
    print("literal pool substitutions:", [f"0x{x:08x}" for x in sorted(all_literal_substitutions)])
    print("unique unresolved LOAD addresses:", [f"0x{x:08x}" for x in sorted(unresolved_addresses)])
    print("affects routing: no")

    if args.details_output is not None:
        args.details_output.parent.mkdir(parents=True, exist_ok=True)
        with args.details_output.open("w", encoding="utf-8") as handle:
            for row in details:
                handle.write(json.dumps(row, sort_keys=True) + "\n")
        print("details output:", args.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
