#!/usr/bin/env python3
"""Fold the address-input SSA of remaining LOAD-root receivers.

Evidence-only. This consumes bounded structural receiver slices plus address
classification memory words. It evaluates only the address subtree of canonical
receiver LOADs. No receiver class is inferred and no routing query is emitted.

Important P-code semantic: an ``address:...`` token is a RAM/storage Varnode,
not an integer address literal. Its value is therefore read from the exact
classified memory word at that storage address. Only ``const:...`` nodes are
used as immediate numeric constants.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

SLICE_FORMAT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
CLASS_FORMAT = "NEVERGONE.GhidraAddressClassification/1"
OUT_FORMAT = "NEVERGONE.LoadAddressExpressionFoldAudit/1"
MASK32 = 0xFFFFFFFF


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            rows.append(row)
    return rows


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def dec(value: Any, default: int = -1) -> int:
    try:
        return int(str(value), 10)
    except Exception:
        return default


def numeric(value: Any) -> int | None:
    if value in (None, "", "-"):
        return None
    text = str(value).strip()
    try:
        if text.lower().startswith("0x"):
            return int(text, 16)
        if re.fullmatch(r"-?\d+", text):
            return int(text, 10)
    except ValueError:
        return None
    return None


def address_from_token(token: Any) -> int | None:
    if not isinstance(token, str):
        return None
    match = re.fullmatch(r"address:([0-9a-fA-F]+):\d+", token.strip())
    return int(match.group(1), 16) if match else None


def safe_id(source_id: str) -> str:
    return source_id.replace(":", "_").replace("/", "_")


def children_for(nodes: dict[int, dict[str, str]]) -> dict[int, list[int]]:
    children: dict[int, list[int]] = defaultdict(list)
    for node_id, row in nodes.items():
        parent = dec(row.get("parent_node_id"))
        if parent >= 0:
            children[parent].append(node_id)
    for parent in children:
        children[parent].sort(key=lambda node_id: (dec(nodes[node_id].get("input_index")), node_id))
    return children


def fold_node(
    node_id: int,
    nodes: dict[int, dict[str, str]],
    children: dict[int, list[int]],
    memory_u32: dict[int, int],
    memo: dict[int, int | None],
    unresolved_reads: set[int],
    resolved_reads: set[int],
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
    child_ids = children.get(node_id, [])
    const = numeric(row.get("constant_value"))
    token_address = address_from_token(row.get("node_token"))

    def child(index: int) -> int | None:
        return fold_node(
            child_ids[index], nodes, children, memory_u32, memo,
            unresolved_reads, resolved_reads, active,
        )

    value: int | None = None
    if const is not None:
        value = const & MASK32
    elif opcode == "-" and token_address is not None:
        # A RAM/address-space Varnode denotes the value stored at that address.
        # It is not an integer address literal. Fail closed if that exact word
        # was not independently classified/exported.
        if token_address in memory_u32:
            resolved_reads.add(token_address)
            value = memory_u32[token_address] & MASK32
        else:
            unresolved_reads.add(token_address)
    elif opcode in {"COPY", "CAST", "INDIRECT"} and child_ids:
        value = child(0)
    elif opcode == "INT_ADD" and len(child_ids) >= 2:
        a, b = child(0), child(1)
        if a is not None and b is not None:
            value = (a + b) & MASK32
    elif opcode == "INT_SUB" and len(child_ids) >= 2:
        a, b = child(0), child(1)
        if a is not None and b is not None:
            value = (a - b) & MASK32
    elif opcode == "PTRADD" and len(child_ids) >= 3:
        base, index, scale = child(0), child(1), child(2)
        if base is not None and index is not None and scale is not None:
            value = (base + index * scale) & MASK32
    elif opcode == "PTRSUB" and len(child_ids) >= 2:
        base, offset = child(0), child(1)
        if base is not None and offset is not None:
            value = (base + offset) & MASK32
    elif opcode == "LOAD" and child_ids:
        address = child(0)
        if address is not None:
            if address in memory_u32:
                resolved_reads.add(address)
                value = memory_u32[address] & MASK32
            else:
                unresolved_reads.add(address)
    elif opcode == "MULTIEQUAL" and child_ids:
        values = [child(i) for i in range(len(child_ids))]
        if values and all(item is not None for item in values) and len(set(values)) == 1:
            value = values[0]

    active.remove(node_id)
    memo[node_id] = value
    return value


def subtree(root: int, children: dict[int, list[int]]) -> list[int]:
    out: list[int] = []
    stack = [root]
    while stack:
        node_id = stack.pop()
        out.append(node_id)
        stack.extend(reversed(children.get(node_id, [])))
    return out


def fmt(value: int | None) -> str:
    return "-" if value is None else f"0x{value:08x}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("failure_jsonl", type=Path)
    parser.add_argument("slice_dir", type=Path)
    parser.add_argument("classification_tsv", type=Path, nargs="+")
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    memory_u32: dict[int, int] = {}
    for path in args.classification_tsv:
        for line_no, row in enumerate(read_tsv(path), 2):
            if row.get("format") != CLASS_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {CLASS_FORMAT}")
            address = numeric("0x" + str(row.get("address", "")).removeprefix("0x"))
            raw = numeric(row.get("raw_u32"))
            if address is not None and raw is not None:
                memory_u32[address & MASK32] = raw & MASK32

    selected = [row for row in read_jsonl(args.failure_jsonl) if row.get("status") == "receiver_not_single_pointer"]
    details: list[dict[str, Any]] = []
    statuses = Counter()

    print("format:", OUT_FORMAT)
    print("candidate receiver_not_single_pointer rows:", len(selected))
    print("memory words available:", len(memory_u32))

    for failure in selected:
        source_id = failure.get("source_id")
        call_index = failure.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            continue
        path = args.slice_dir / f"{safe_id(source_id)}.tsv"
        if not path.is_file():
            status = "missing_slice_file"
            statuses[status] += 1
            details.append({"format": OUT_FORMAT, "source_id": source_id, "call_index": call_index, "status": status, "affects_routing": False})
            continue

        rows = read_tsv(path)
        for line_no, row in enumerate(rows, 2):
            if row.get("format") != SLICE_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {SLICE_FORMAT}")
        call_rows = [row for row in rows if dec(row.get("call_index")) == call_index]
        node_rows = [row for row in call_rows if row.get("row_type") == "node"]
        nodes = {dec(row.get("node_id")): row for row in node_rows if dec(row.get("node_id")) >= 0}
        root = nodes.get(0)
        if root is None or root.get("defining_opcode") != "LOAD":
            status = "root_not_load"
            statuses[status] += 1
            details.append({"format": OUT_FORMAT, "source_id": source_id, "call_index": call_index, "status": status, "affects_routing": False})
            continue

        children = children_for(nodes)
        address_children = children.get(0, [])
        if len(address_children) != 1:
            status = "load_address_child_not_unique"
            statuses[status] += 1
            details.append({"format": OUT_FORMAT, "source_id": source_id, "call_index": call_index, "status": status, "address_child_count": len(address_children), "affects_routing": False})
            continue

        address_root = address_children[0]
        memo: dict[int, int | None] = {}
        unresolved_reads: set[int] = set()
        resolved_reads: set[int] = set()
        effective = fold_node(address_root, nodes, children, memory_u32, memo, unresolved_reads, resolved_reads, set())
        if effective is not None:
            status = "effective_address_folded"
        elif unresolved_reads:
            status = "unresolved_memory_read"
        else:
            status = "unsupported_address_expression"
        statuses[status] += 1

        tree_rows: list[dict[str, Any]] = []
        for node_id in subtree(address_root, children):
            row = nodes[node_id]
            value = fold_node(node_id, nodes, children, memory_u32, memo, unresolved_reads, resolved_reads, set())
            tree_rows.append({
                "node_id": node_id,
                "parent_node_id": dec(row.get("parent_node_id")),
                "input_index": dec(row.get("input_index")),
                "depth": dec(row.get("depth"), 0),
                "opcode": row.get("defining_opcode"),
                "token": row.get("node_token"),
                "constant_value": row.get("constant_value"),
                "folded_value": value,
            })

        detail = {
            "format": OUT_FORMAT,
            "source_id": source_id,
            "call_index": call_index,
            "call_address": failure.get("call_address"),
            "dispatch_slot_offset": failure.get("dispatch_slot_offset"),
            "address_root_node_id": address_root,
            "address_root_opcode": nodes[address_root].get("defining_opcode"),
            "effective_address": effective,
            "resolved_memory_reads": sorted(resolved_reads),
            "unresolved_memory_reads": sorted(unresolved_reads),
            "tree": tree_rows,
            "status": status,
            "affects_routing": False,
        }
        details.append(detail)

        print("\n", source_id, "call", call_index, "slot=", failure.get("dispatch_slot_offset"), "status=", status, sep="")
        print("address root:", address_root, nodes[address_root].get("defining_opcode"), "effective=", fmt(effective))
        print("resolved memory reads:", [f"0x{x:08x}" for x in sorted(resolved_reads)])
        print("unresolved memory reads:", [f"0x{x:08x}" for x in sorted(unresolved_reads)])
        print("address tree:")
        base_depth = dec(nodes[address_root].get("depth"), 0)
        for item in tree_rows:
            indent = "  " * max(0, item["depth"] - base_depth)
            print(f"{indent}node={item['node_id']} input={item['input_index']} op={item['opcode']} token={item['token']} const={item['constant_value']} folded={fmt(item['folded_value'])}")

    print("\nstatus counts:", dict(statuses))
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
