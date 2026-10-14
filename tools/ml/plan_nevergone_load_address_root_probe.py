#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

SLICE_FORMAT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
PLAN_FORMAT = "NEVERGONE.LoadAddressRootProbePlan/1"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        return [json.loads(line) for line in handle if line.strip()]


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def dec(value: Any, default: int = -1) -> int:
    try:
        return int(str(value), 10)
    except Exception:
        return default


def safe_id(source_id: str) -> str:
    return source_id.replace(":", "_").replace("/", "_")


def address_from_token(token: str | None) -> str | None:
    if not isinstance(token, str):
        return None
    match = re.fullmatch(r"address:([0-9a-fA-F]+):\d+", token.strip())
    if not match:
        return None
    return f"{int(match.group(1), 16):08x}"


def descendants(root_id: int, children: dict[int, list[int]]) -> set[int]:
    seen: set[int] = set()
    stack = [root_id]
    while stack:
        current = stack.pop()
        if current in seen:
            continue
        seen.add(current)
        stack.extend(children.get(current, []))
    return seen


def main() -> int:
    parser = argparse.ArgumentParser(description="Plan evidence-only classification of unresolved LOAD address roots")
    parser.add_argument("failure_jsonl", type=Path)
    parser.add_argument("slice_dir", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    out: list[dict[str, Any]] = []
    skips = Counter()
    skip_details: list[dict[str, Any]] = []
    candidates = 0
    for failure in read_jsonl(args.failure_jsonl):
        if failure.get("status") != "receiver_not_single_pointer":
            continue
        candidates += 1
        source_id = failure.get("source_id")
        call_index = failure.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            skips["invalid_failure_key"] += 1
            continue
        path = args.slice_dir / f"{safe_id(source_id)}.tsv"
        if not path.is_file():
            skips["missing_slice_file"] += 1
            skip_details.append({"source_id": source_id, "call_index": call_index, "reason": "missing_slice_file"})
            continue
        rows = read_tsv(path)
        for line_no, row in enumerate(rows, 2):
            if row.get("format") != SLICE_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {SLICE_FORMAT}")
        call_rows = [r for r in rows if dec(r.get("call_index")) == call_index]
        summary = next((r for r in call_rows if r.get("row_type") == "summary"), None)
        nodes = [r for r in call_rows if r.get("row_type") == "node"]
        nodes_by_id = {dec(r.get("node_id")): r for r in nodes if dec(r.get("node_id")) >= 0}
        children: dict[int, list[int]] = defaultdict(list)
        for row in nodes:
            node_id = dec(row.get("node_id"))
            parent_id = dec(row.get("parent_node_id"))
            if node_id >= 0 and parent_id >= 0:
                children[parent_id].append(node_id)
        root = nodes_by_id.get(0)
        if summary is None:
            skips["missing_call_summary"] += 1
            skip_details.append({"source_id": source_id, "call_index": call_index, "reason": "missing_call_summary"})
            continue
        if root is None:
            skips["missing_root_node"] += 1
            skip_details.append({"source_id": source_id, "call_index": call_index, "reason": "missing_root_node"})
            continue
        if root.get("defining_opcode") != "LOAD":
            skips["root_not_load"] += 1
            skip_details.append({
                "source_id": source_id,
                "call_index": call_index,
                "reason": "root_not_load",
                "root_opcode": root.get("defining_opcode"),
                "root_token": root.get("node_token"),
            })
            continue

        # ExportIndirectCallStructuralReceiverSlice omits the LOAD space-id input,
        # so the root should have exactly one data child: the address expression.
        address_roots = [r for r in nodes if dec(r.get("parent_node_id")) == 0 and dec(r.get("input_index")) == 1]
        if len(address_roots) != 1:
            skips["load_address_root_not_unique"] += 1
            skip_details.append({
                "source_id": source_id,
                "call_index": call_index,
                "reason": "load_address_root_not_unique",
                "direct_children": [
                    {"node_id": dec(r.get("node_id")), "input_index": dec(r.get("input_index")), "opcode": r.get("defining_opcode"), "token": r.get("node_token")}
                    for r in nodes if dec(r.get("parent_node_id")) == 0
                ],
            })
            continue

        address_root = address_roots[0]
        address_root_id = dec(address_root.get("node_id"))
        subtree_ids = descendants(address_root_id, children)
        subtree_rows = [nodes_by_id[node_id] for node_id in sorted(subtree_ids) if node_id in nodes_by_id]
        terminal_rows = [r for r in subtree_rows if not children.get(dec(r.get("node_id")))]
        address_leaves: list[tuple[dict[str, str], str]] = []
        for leaf in terminal_rows:
            address = address_from_token(leaf.get("node_token"))
            if address is not None and leaf.get("defining_opcode") in {None, "", "-"}:
                address_leaves.append((leaf, address))

        if not address_leaves:
            skips["no_terminal_address_leaf"] += 1
            skip_details.append({
                "source_id": source_id,
                "call_index": call_index,
                "reason": "no_terminal_address_leaf",
                "address_root": {"node_id": address_root_id, "opcode": address_root.get("defining_opcode"), "token": address_root.get("node_token")},
                "terminal_nodes": [
                    {"node_id": dec(r.get("node_id")), "opcode": r.get("defining_opcode"), "token": r.get("node_token"), "constant": r.get("constant_value")}
                    for r in terminal_rows
                ],
            })
            continue
        if len(address_leaves) != 1:
            skips["ambiguous_terminal_address_leaves"] += 1
            skip_details.append({
                "source_id": source_id,
                "call_index": call_index,
                "reason": "ambiguous_terminal_address_leaves",
                "address_root": {"node_id": address_root_id, "opcode": address_root.get("defining_opcode"), "token": address_root.get("node_token")},
                "address_leaves": [
                    {"node_id": dec(r.get("node_id")), "token": r.get("node_token"), "address": address}
                    for r, address in address_leaves
                ],
            })
            continue

        leaf, root_address = address_leaves[0]
        out.append({
            "format": PLAN_FORMAT,
            "source_id": source_id,
            "call_index": call_index,
            "call_address": failure.get("call_address"),
            "dispatch_slot_offset": failure.get("dispatch_slot_offset"),
            "function_entry": summary.get("function_entry"),
            "recovery_status": summary.get("recovery_status"),
            "root_opcode": root.get("defining_opcode"),
            "root_token": root.get("node_token"),
            "address_root_node_id": address_root_id,
            "address_root_opcode": address_root.get("defining_opcode"),
            "address_root_token": address_root.get("node_token"),
            "address_leaf_node_id": dec(leaf.get("node_id")),
            "address_leaf_token": leaf.get("node_token"),
            "root_address": root_address,
            "affects_routing": False,
        })

    args.output.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "format", "source_id", "call_index", "call_address", "dispatch_slot_offset",
        "function_entry", "recovery_status", "root_opcode", "root_token",
        "address_root_node_id", "address_root_opcode", "address_root_token",
        "address_leaf_node_id", "address_leaf_token", "root_address", "affects_routing",
    ]
    with args.output.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(out)

    print("format:", PLAN_FORMAT)
    print("candidate receiver_not_single_pointer rows:", candidates)
    print("planned LOAD address roots:", len(out))
    print("skip counts:", dict(skips.most_common()))
    for row in out:
        print(
            row["source_id"], "call", row["call_index"],
            "slot=", row["dispatch_slot_offset"],
            "address_root=", row["address_root_opcode"], row["address_root_token"],
            "root_address=", row["root_address"],
        )
    if skip_details:
        print("skip details:")
        for detail in skip_details:
            print(json.dumps(detail, sort_keys=True))
    print("output:", args.output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
