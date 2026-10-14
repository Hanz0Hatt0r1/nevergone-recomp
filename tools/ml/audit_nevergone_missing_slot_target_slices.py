#!/usr/bin/env python3
"""Inspect bounded CALLIND target slices for rows missing dispatch slot offsets.

Evidence-only diagnostic. Reads the persistent ExportIndirectCallSlice TSVs and
prints the exact bounded P-code target tree for inference rows whose status is
missing_slot_offset. It does not infer a slot or modify routing.
"""
from __future__ import annotations

import argparse
import csv
import json
from collections import defaultdict
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"


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


def safe_id(source_id: str) -> str:
    return source_id.replace(":", "_").replace("/", "_")


def render_node(row: dict[str, str]) -> str:
    node_id = parse_int(row.get("node_id"))
    parent = parse_int(row.get("parent_node_id"))
    input_index = parse_int(row.get("input_index"))
    depth = parse_int(row.get("depth"), 0)
    opcode = row.get("defining_opcode") or "-"
    constant = row.get("constant_value") or "-"
    token = row.get("node_token") or "-"
    def_addr = row.get("defining_address") or "-"
    def_time = row.get("defining_time") or "-"
    trunc = row.get("truncation") or "-"
    return (
        f"node={node_id} parent={parent} input={input_index} depth={depth} "
        f"opcode={opcode} const={constant} token={token} "
        f"def={def_addr}:{def_time} trunc={trunc}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("slice_dir", type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    inference = read_jsonl(args.inference_jsonl)
    targets = [row for row in inference if row.get("status") == "missing_slot_offset"]

    details: list[dict[str, Any]] = []
    missing_slice_files = 0
    missing_call_slices = 0

    print("format: NEVERGONE.MissingSlotTargetSliceAudit/1")
    print("inference rows:", len(inference))
    print("missing slot rows:", len(targets))

    for inf in targets:
        if inf.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"expected {INFERENCE_FORMAT}")
        source_id = inf.get("source_id")
        call_index = inf.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            continue

        path = args.slice_dir / f"{safe_id(source_id)}.tsv"
        print()
        print(f"target: {source_id} call {call_index}")
        print("receiver class:", inf.get("receiver_class_name"))
        print("receiver type:", inf.get("receiver_type_display"))
        print("call address:", inf.get("call_address"))
        print("slice file:", path)

        detail: dict[str, Any] = {
            "source_id": source_id,
            "call_index": call_index,
            "call_address": inf.get("call_address"),
            "receiver_class_name": inf.get("receiver_class_name"),
            "receiver_type_display": inf.get("receiver_type_display"),
            "slice_file": str(path),
            "affects_routing": False,
        }

        if not path.is_file():
            print("slice status: missing_file")
            detail["slice_status"] = "missing_file"
            missing_slice_files += 1
            details.append(detail)
            continue

        rows = read_tsv(path)
        selected = [row for row in rows if parse_int(row.get("call_index")) == call_index]
        summary = next((row for row in selected if row.get("row_type") == "summary"), None)
        nodes = [row for row in selected if row.get("row_type") == "node"]
        nodes.sort(key=lambda row: parse_int(row.get("node_id"), 1 << 30))

        if summary is None and not nodes:
            print("slice status: missing_call")
            detail["slice_status"] = "missing_call"
            missing_call_slices += 1
            details.append(detail)
            continue

        print("slice status: present")
        if summary is not None:
            print("node count:", summary.get("node_count"))
            print("truncated by budget:", summary.get("truncated_by_budget"))
            print("depth truncation count:", summary.get("depth_truncation_count"))
            print("cycle count:", summary.get("cycle_count"))
            print("arg count:", summary.get("arg_count"))

        opcodes: dict[str, int] = defaultdict(int)
        constants: list[dict[str, Any]] = []
        arithmetic_nodes: list[dict[str, Any]] = []
        for row in nodes:
            opcode = row.get("defining_opcode") or "-"
            if opcode not in {"", "-"}:
                opcodes[opcode] += 1
            constant = row.get("constant_value")
            if constant not in {None, "", "-"}:
                constants.append({
                    "node_id": parse_int(row.get("node_id")),
                    "parent_node_id": parse_int(row.get("parent_node_id")),
                    "input_index": parse_int(row.get("input_index")),
                    "depth": parse_int(row.get("depth"), 0),
                    "value": constant,
                })
            if opcode in {"PTRADD", "PTRSUB", "INT_ADD", "INT_SUB", "LOAD", "COPY", "CAST", "INDIRECT", "MULTIEQUAL"}:
                arithmetic_nodes.append({
                    "node_id": parse_int(row.get("node_id")),
                    "parent_node_id": parse_int(row.get("parent_node_id")),
                    "input_index": parse_int(row.get("input_index")),
                    "depth": parse_int(row.get("depth"), 0),
                    "opcode": opcode,
                    "token": row.get("node_token"),
                })

        print("opcode counts:", dict(sorted(opcodes.items())))
        print("constants:", constants)
        print("nodes:")
        for row in nodes:
            print(" ", render_node(row))

        detail.update({
            "slice_status": "present",
            "summary": summary,
            "opcode_counts": dict(sorted(opcodes.items())),
            "constants": constants,
            "interesting_nodes": arithmetic_nodes,
            "nodes": nodes,
        })
        details.append(detail)

    print()
    print("missing slice files:", missing_slice_files)
    print("missing call slices:", missing_call_slices)
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
