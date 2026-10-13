#!/usr/bin/env python3
"""Attach compact summaries of bounded CALLIND provenance slices."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.IndirectCallSliceEvidence/1"
ARITHMETIC_OPS = {"PTRADD", "PTRSUB", "INT_ADD", "INT_SUB"}


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


def safe_id(record_id: str) -> str:
    return record_id.replace(":", "_").replace("/", "_")


def parse_int(value: str | None, default: int = 0) -> int:
    if value in {None, "", "-"}:
        return default
    return int(value)


def token_kind(token: str) -> str:
    return token.split(":", 1)[0] if token else "empty"


def canonical_shape(
    node_id: int,
    nodes: dict[int, dict[str, str]],
    children: dict[int, list[int]],
    visiting: set[int] | None = None,
) -> str:
    visiting = set() if visiting is None else set(visiting)
    if node_id in visiting:
        return "CYCLE"
    visiting.add(node_id)
    node = nodes[node_id]
    constant = node.get("constant_value", "-")
    if constant not in {"", "-"}:
        return "CONST"

    opcode = node.get("defining_opcode", "-")
    truncation = node.get("truncation", "-")
    child_ids = children.get(node_id, [])
    if opcode not in {"", "-"}:
        child_shapes = [canonical_shape(child, nodes, children, visiting) for child in child_ids]
        shape = opcode + ("(" + ",".join(child_shapes) + ")" if child_shapes else "")
    else:
        shape = token_kind(node.get("node_token", ""))
    if truncation not in {"", "-"}:
        shape += f"[{truncation.upper()}]"
    return shape


def call_summary(
    summary: dict[str, str], node_rows: list[dict[str, str]], small_constant_max: int
) -> dict[str, Any]:
    nodes: dict[int, dict[str, str]] = {}
    children: dict[int, list[int]] = defaultdict(list)
    operators: Counter[str] = Counter()
    leaf_kinds: Counter[str] = Counter()

    for row in node_rows:
        node_id = parse_int(row.get("node_id"), -1)
        if node_id < 0:
            continue
        nodes[node_id] = row
        parent = parse_int(row.get("parent_node_id"), -1)
        if parent >= 0:
            children[parent].append(node_id)
        opcode = row.get("defining_opcode", "-")
        if opcode not in {"", "-"}:
            operators[opcode] += 1
        elif row.get("constant_value", "-") in {"", "-"}:
            leaf_kinds[token_kind(row.get("node_token", ""))] += 1

    def child_sort_key(node_id: int) -> tuple[int, int]:
        row = nodes[node_id]
        return (parse_int(row.get("input_index"), 999999), node_id)

    for parent in list(children):
        children[parent].sort(key=child_sort_key)

    root_id = 0 if 0 in nodes else (min(nodes) if nodes else -1)
    shape = canonical_shape(root_id, nodes, children) if root_id >= 0 else "EMPTY"
    shape_hash = hashlib.sha256(shape.encode("utf-8")).hexdigest()[:16]

    offset_candidates: list[tuple[int, int]] = []
    all_small_constants: Counter[str] = Counter()
    for node_id, row in nodes.items():
        constant = row.get("constant_value", "-")
        if constant in {"", "-"}:
            continue
        try:
            value = int(constant, 16)
        except ValueError:
            continue
        if value <= small_constant_max:
            all_small_constants[f"0x{value:x}"] += 1
        parent_id = parse_int(row.get("parent_node_id"), -1)
        parent = nodes.get(parent_id)
        if parent and parent.get("defining_opcode") in ARITHMETIC_OPS and value <= small_constant_max:
            offset_candidates.append((parse_int(row.get("depth"), 999999), value))

    offset_candidates.sort(key=lambda item: (item[0], item[1]))
    unique_offsets: list[str] = []
    seen_offsets: set[int] = set()
    for _, value in offset_candidates:
        if value in seen_offsets:
            continue
        seen_offsets.add(value)
        unique_offsets.append(f"0x{value:x}")
    primary_offset = unique_offsets[0] if unique_offsets else None

    budget_truncated = bool(parse_int(summary.get("truncated_by_budget"), 0))
    depth_truncations = parse_int(summary.get("depth_truncation_count"), 0)
    cycle_count = parse_int(summary.get("cycle_count"), 0)
    complete = not budget_truncated and depth_truncations == 0

    return {
        "call_index": parse_int(summary.get("call_index"), 0),
        "call_address": summary.get("call_address"),
        "arg_count": parse_int(summary.get("arg_count"), 0),
        "node_count": parse_int(summary.get("node_count"), len(nodes)),
        "max_nodes": parse_int(summary.get("max_nodes"), 0),
        "max_depth": parse_int(summary.get("max_depth"), 0),
        "budget_truncated": budget_truncated,
        "depth_truncation_count": depth_truncations,
        "cycle_count": cycle_count,
        "complete": complete,
        "root_opcode": nodes.get(root_id, {}).get("defining_opcode") if root_id >= 0 else None,
        "operator_counts": dict(sorted(operators.items())),
        "leaf_kind_counts": dict(sorted(leaf_kinds.items())),
        "nested_load": operators.get("LOAD", 0) >= 2,
        "shape_hash": shape_hash,
        "shape_preview": shape[:240],
        "slot_offset_candidates": unique_offsets,
        "primary_slot_offset_candidate": primary_offset,
        "small_constant_counts": dict(sorted(all_small_constants.items(), key=lambda item: int(item[0], 16))),
        "slot_signature": f"{shape_hash}@{primary_offset or 'none'}",
    }


def summarize_file(path: Path, small_constant_max: int) -> dict[str, Any]:
    rows = read_tsv(path)
    summaries: dict[int, dict[str, str]] = {}
    node_rows: dict[int, list[dict[str, str]]] = defaultdict(list)
    for row in rows:
        call_index = parse_int(row.get("call_index"), -1)
        if call_index < 0:
            continue
        if row.get("row_type") == "summary":
            summaries[call_index] = row
        elif row.get("row_type") == "node":
            node_rows[call_index].append(row)

    calls: list[dict[str, Any]] = []
    for call_index in sorted(set(summaries) | set(node_rows)):
        summary = summaries.get(call_index, {"call_index": str(call_index)})
        calls.append(call_summary(summary, node_rows.get(call_index, []), small_constant_max))

    offsets: Counter[str] = Counter()
    signatures: Counter[str] = Counter()
    shapes: Counter[str] = Counter()
    leaf_kinds: Counter[str] = Counter()
    operator_counts: Counter[str] = Counter()
    for call in calls:
        if call["primary_slot_offset_candidate"]:
            offsets[call["primary_slot_offset_candidate"]] += 1
        signatures[call["slot_signature"]] += 1
        shapes[call["shape_hash"]] += 1
        leaf_kinds.update(call["leaf_kind_counts"])
        operator_counts.update(call["operator_counts"])

    return {
        "format": FORMAT,
        "present": True,
        "call_count": len(calls),
        "complete_call_count": sum(int(call["complete"]) for call in calls),
        "budget_truncated_call_count": sum(int(call["budget_truncated"]) for call in calls),
        "depth_truncated_call_count": sum(int(call["depth_truncation_count"] > 0) for call in calls),
        "cyclic_call_count": sum(int(call["cycle_count"] > 0) for call in calls),
        "nested_load_call_count": sum(int(call["nested_load"]) for call in calls),
        "primary_slot_offset_counts": dict(sorted(offsets.items(), key=lambda item: int(item[0], 16))),
        "shape_hash_counts": dict(sorted(shapes.items())),
        "slot_signature_counts": dict(sorted(signatures.items())),
        "leaf_kind_counts": dict(sorted(leaf_kinds.items())),
        "operator_counts": dict(sorted(operator_counts.items())),
        "calls": calls,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    parser.add_argument("evidence_root", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--small-constant-max", type=lambda value: int(value, 0), default=0x1000)
    args = parser.parse_args()
    if args.small_constant_max < 0:
        raise SystemExit("--small-constant-max must be non-negative")

    rows = read_jsonl(args.analysis)
    out_rows: list[dict[str, Any]] = []
    present = 0
    missing_requested = 0
    aggregate_offsets: Counter[str] = Counter()
    aggregate_signatures: Counter[str] = Counter()
    complete_calls = 0
    budget_truncated_calls = 0
    depth_truncated_calls = 0

    slice_dir = args.evidence_root / "indirect-slices"
    for source in rows:
        record_id = source.get("id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("analysis row missing id")
        before_action = source.get("final_action")
        pattern = source.get("indirect_call_pattern_evidence")
        requested = (
            isinstance(pattern, dict)
            and bool(pattern.get("present"))
            and int(pattern.get("depth_limited_expression_count", 0) or 0) > 0
        )
        path = slice_dir / (safe_id(record_id) + ".tsv")
        if path.is_file():
            evidence = summarize_file(path, args.small_constant_max)
            evidence["requested"] = requested
            present += 1
            aggregate_offsets.update(evidence["primary_slot_offset_counts"])
            aggregate_signatures.update(evidence["slot_signature_counts"])
            complete_calls += evidence["complete_call_count"]
            budget_truncated_calls += evidence["budget_truncated_call_count"]
            depth_truncated_calls += evidence["depth_truncated_call_count"]
        else:
            evidence = {"format": FORMAT, "requested": requested, "present": False}
            missing_requested += int(requested)

        row = dict(source)
        row["indirect_call_slice_evidence"] = evidence
        if row.get("final_action") != before_action:
            raise AssertionError(f"{record_id}: final_action changed")
        out_rows.append(row)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in out_rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    print("format:", FORMAT)
    print("functions:", len(out_rows))
    print("slice evidence present:", present)
    print("missing requested:", missing_requested)
    print("complete calls:", complete_calls)
    print("budget-truncated calls:", budget_truncated_calls)
    print("depth-truncated calls:", depth_truncated_calls)
    print("primary offsets:", dict(aggregate_offsets))
    print("unique slot signatures:", len(aggregate_signatures))
    print("final_action unchanged: yes")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
