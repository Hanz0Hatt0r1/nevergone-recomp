#!/usr/bin/env python3
"""Audit bounded structural receiver P-code slices for unresolved non-LOAD roots.

Evidence-only. It expands MULTIEQUAL branches and reports terminal definitions,
types, catalog mappings, and direct CALL metadata. It does not infer a receiver
class, emit a query, or change routing.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

ROOT_AUDIT_FORMAT = "NEVERGONE.NonLoadStructuralRootAudit/1"
SLICE_FORMAT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
AUDIT_FORMAT = "NEVERGONE.StructuralReceiverSliceAudit/1"


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


def parse_int(value: Any, default: int = -1) -> int:
    if value in (None, "", "-"):
        return default
    try:
        return int(str(value), 0)
    except ValueError:
        return default


def clean_type(value: Any) -> str | None:
    if not isinstance(value, str):
        return None
    text = value.strip()
    if not text or text == "-":
        return None
    text = re.sub(r"^(?:class|struct)\s+", "", text)
    text = re.sub(r"\s+", " ", text).strip()
    while text.endswith(" *"):
        text = text[:-2].rstrip()
    return text or None


def terminal(name: str) -> str:
    return name.rsplit("::", 1)[-1]


def map_type(value: Any, catalog: set[str], by_terminal: dict[str, set[str]]) -> set[str]:
    cleaned = clean_type(value)
    if cleaned is None:
        return set()
    if cleaned in catalog:
        return {cleaned}
    matches = by_terminal.get(terminal(cleaned), set())
    return set(matches) if len(matches) == 1 else set()


def safe_id(source_id: str) -> str:
    return source_id.replace(":", "_").replace("/", "_")


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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root_audit_jsonl", type=Path)
    parser.add_argument("slice_dir", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    root_rows = read_jsonl(args.root_audit_jsonl)
    catalog_rows = read_jsonl(args.catalog_jsonl)

    catalog: set[str] = set()
    for line_no, row in enumerate(catalog_rows, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{args.catalog_jsonl}:{line_no}: expected {CATALOG_FORMAT}")
        name = row.get("class_name")
        if isinstance(name, str) and name:
            catalog.add(name)
    by_terminal: dict[str, set[str]] = defaultdict(set)
    for name in catalog:
        by_terminal[terminal(name)].add(name)

    selected: list[dict[str, Any]] = []
    for line_no, row in enumerate(root_rows, 1):
        if row.get("format") != ROOT_AUDIT_FORMAT:
            raise ValueError(f"{args.root_audit_jsonl}:{line_no}: expected {ROOT_AUDIT_FORMAT}")
        if row.get("bucket") == "receiver_root_not_parameter":
            selected.append(row)

    slice_cache: dict[str, list[dict[str, str]]] = {}
    missing_files = 0
    missing_calls = 0
    root_opcodes = Counter()
    result_buckets = Counter()
    details: list[dict[str, Any]] = []

    for selected_row in selected:
        source_id = selected_row.get("source_id")
        call_index = selected_row.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            continue
        path = args.slice_dir / f"{safe_id(source_id)}.tsv"
        if not path.is_file():
            missing_files += 1
            details.append({
                "format": AUDIT_FORMAT,
                "source_id": source_id,
                "call_index": call_index,
                "status": "missing_slice_file",
                "affects_routing": False,
            })
            continue
        if source_id not in slice_cache:
            slice_cache[source_id] = read_tsv(path)
        file_rows = slice_cache[source_id]
        for line_no, row in enumerate(file_rows, 2):
            if row.get("format") != SLICE_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {SLICE_FORMAT}")

        call_rows = [r for r in file_rows if parse_int(r.get("call_index"), -1) == call_index]
        summary = next((r for r in call_rows if r.get("row_type") == "summary"), None)
        nodes = [r for r in call_rows if r.get("row_type") == "node"]
        if summary is None or not nodes:
            missing_calls += 1
            details.append({
                "format": AUDIT_FORMAT,
                "source_id": source_id,
                "call_index": call_index,
                "status": "missing_call_slice",
                "affects_routing": False,
            })
            continue

        nodes_by_id = {parse_int(r.get("node_id")): r for r in nodes}
        children: dict[int, list[int]] = defaultdict(list)
        for row in nodes:
            node_id = parse_int(row.get("node_id"))
            parent = parse_int(row.get("parent_node_id"))
            if node_id >= 0 and parent >= 0:
                children[parent].append(node_id)
        root = nodes_by_id.get(0)
        root_opcode = root.get("defining_opcode") if root else "-"
        root_opcodes[root_opcode] += 1

        leaf_ids = sorted(node_id for node_id in nodes_by_id if not children.get(node_id))
        leaf_classes: set[str] = set()
        for node_id in leaf_ids:
            leaf_classes.update(map_type(nodes_by_id[node_id].get("pointee_type_display"), catalog, by_terminal))

        direct_branch_ids = sorted(children.get(0, []), key=lambda node_id: parse_int(nodes_by_id[node_id].get("input_index")))
        branches: list[dict[str, Any]] = []
        branch_class_sets: list[set[str]] = []
        for branch_id in direct_branch_ids:
            ids = descendants(branch_id, children)
            branch_leaf_ids = sorted(node_id for node_id in ids if not children.get(node_id))
            mapped: set[str] = set()
            for node_id in ids:
                mapped.update(map_type(nodes_by_id[node_id].get("pointee_type_display"), catalog, by_terminal))
            branch_class_sets.append(mapped)
            branches.append({
                "input_index": parse_int(nodes_by_id[branch_id].get("input_index")),
                "root_node_id": branch_id,
                "root_opcode": nodes_by_id[branch_id].get("defining_opcode"),
                "root_type_display": nodes_by_id[branch_id].get("type_display"),
                "root_pointee_type_display": nodes_by_id[branch_id].get("pointee_type_display"),
                "mapped_catalog_classes": sorted(mapped),
                "leaf_nodes": [
                    {
                        "node_id": node_id,
                        "opcode": nodes_by_id[node_id].get("defining_opcode"),
                        "type_display": nodes_by_id[node_id].get("type_display"),
                        "pointer_depth": parse_int(nodes_by_id[node_id].get("pointer_depth"), 0),
                        "pointee_type_display": nodes_by_id[node_id].get("pointee_type_display"),
                        "token": nodes_by_id[node_id].get("node_token"),
                        "call_target_entry": nodes_by_id[node_id].get("call_target_entry"),
                        "call_target_name": nodes_by_id[node_id].get("call_target_name"),
                        "call_target_return_type": nodes_by_id[node_id].get("call_target_return_type"),
                        "call_target_is_thunk": nodes_by_id[node_id].get("call_target_is_thunk"),
                        "call_target_thunk_target": nodes_by_id[node_id].get("call_target_thunk_target"),
                        "truncation": nodes_by_id[node_id].get("truncation"),
                    }
                    for node_id in branch_leaf_ids
                ],
            })

        call_nodes = [
            {
                "node_id": parse_int(row.get("node_id")),
                "depth": parse_int(row.get("depth")),
                "type_display": row.get("type_display"),
                "pointee_type_display": row.get("pointee_type_display"),
                "target_entry": row.get("call_target_entry"),
                "target_name": row.get("call_target_name"),
                "return_type": row.get("call_target_return_type"),
                "is_thunk": row.get("call_target_is_thunk"),
                "thunk_target": row.get("call_target_thunk_target"),
            }
            for row in nodes if row.get("defining_opcode") == "CALL"
        ]

        if root_opcode == "MULTIEQUAL":
            nonempty = [classes for classes in branch_class_sets if classes]
            union = set().union(*nonempty) if nonempty else set()
            if len(branch_class_sets) >= 2 and len(nonempty) == len(branch_class_sets) and len(union) == 1:
                result_bucket = "phi_branches_converge_one_catalog_class"
            else:
                result_bucket = "phi_requires_deeper_evidence"
        elif root_opcode == "CALL":
            result_bucket = "call_root_requires_return_evidence"
        else:
            result_bucket = "other_root_requires_evidence"
        result_buckets[result_bucket] += 1

        details.append({
            "format": AUDIT_FORMAT,
            "source_id": source_id,
            "call_index": call_index,
            "call_address": selected_row.get("call_address"),
            "dispatch_slot_offset": selected_row.get("dispatch_slot_offset"),
            "slice_file": str(path),
            "recovery_status": summary.get("recovery_status"),
            "slice_slot_offset": summary.get("slot_offset"),
            "root_opcode": root_opcode,
            "root_type_display": root.get("type_display") if root else None,
            "root_pointee_type_display": root.get("pointee_type_display") if root else None,
            "node_count": parse_int(summary.get("node_count"), len(nodes)),
            "truncated_by_budget": parse_int(summary.get("truncated_by_budget"), 0),
            "depth_truncation_count": parse_int(summary.get("depth_truncation_count"), 0),
            "cycle_count": parse_int(summary.get("cycle_count"), 0),
            "leaf_catalog_classes": sorted(leaf_classes),
            "branches": branches,
            "call_nodes": call_nodes,
            "result_bucket": result_bucket,
            "affects_routing": False,
        })

    print("format:", AUDIT_FORMAT)
    print("selected receiver roots:", len(selected))
    print("root opcode counts:", dict(root_opcodes.most_common()))
    print("result buckets:", dict(result_buckets.most_common()))
    print("missing slice files:", missing_files)
    print("missing call slices:", missing_calls)

    print("\nrows:")
    for row in details:
        print()
        print("target:", row.get("source_id"), "call", row.get("call_index"), "slot=", row.get("dispatch_slot_offset"))
        print("root:", row.get("root_opcode"), "type=", row.get("root_type_display"), "pointee=", row.get("root_pointee_type_display"))
        print("slice:", "nodes=", row.get("node_count"), "budget_trunc=", row.get("truncated_by_budget"), "depth_trunc=", row.get("depth_truncation_count"), "cycles=", row.get("cycle_count"))
        print("leaf catalog classes:", row.get("leaf_catalog_classes"))
        print("result:", row.get("result_bucket"))
        branches = row.get("branches") or []
        if branches:
            print("branches:")
            for branch in branches:
                print("  input=", branch["input_index"], "node=", branch["root_node_id"], "opcode=", branch["root_opcode"], "type=", branch["root_type_display"], "pointee=", branch["root_pointee_type_display"], "classes=", branch["mapped_catalog_classes"])
                for leaf in branch["leaf_nodes"]:
                    print("    leaf node=", leaf["node_id"], "opcode=", leaf["opcode"], "type=", leaf["type_display"], "depth=", leaf["pointer_depth"], "pointee=", leaf["pointee_type_display"], "token=", leaf["token"], "call=", leaf["call_target_name"], "ret=", leaf["call_target_return_type"], "trunc=", leaf["truncation"])
        call_nodes = row.get("call_nodes") or []
        if call_nodes:
            print("calls:")
            for call in call_nodes:
                print("  node=", call["node_id"], "depth=", call["depth"], "type=", call["type_display"], "target=", call["target_entry"], call["target_name"], "return=", call["return_type"], "thunk=", call["is_thunk"], call["thunk_target"])

    if args.details_output is not None:
        args.details_output.parent.mkdir(parents=True, exist_ok=True)
        with args.details_output.open("w", encoding="utf-8") as handle:
            for row in details:
                handle.write(json.dumps(row, sort_keys=True) + "\n")
        print("\ndetails output:", args.details_output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
