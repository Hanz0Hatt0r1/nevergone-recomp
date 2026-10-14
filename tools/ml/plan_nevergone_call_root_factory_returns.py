#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import re
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
SLICE_FORMAT = "NEVERGONE.GhidraStructuralReceiverSlice/1"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            rows.append(row)
    return rows


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def parse_decimal(value: Any, default: int = -1) -> int:
    if value in (None, "", "-"):
        return default
    text = str(value).strip()
    if not re.fullmatch(r"-?[0-9]+", text):
        raise ValueError(f"expected decimal integer, got {value!r}")
    return int(text, 10)


def source_entry(source_id: str) -> str:
    return source_id.split(":", 1)[1] if ":" in source_id else source_id


def load_slice_roots(slice_dir: Path) -> dict[tuple[str, int], dict[str, str]]:
    roots: dict[tuple[str, int], dict[str, str]] = {}
    for path in sorted(slice_dir.glob("*.tsv")):
        rows = read_tsv(path)
        for line_no, row in enumerate(rows, 2):
            if row.get("format") != SLICE_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {SLICE_FORMAT}")
            if row.get("row_type") != "node":
                continue
            if parse_decimal(row.get("node_id"), -1) != 0:
                continue
            entry = row.get("function_entry") or ""
            call_index = parse_decimal(row.get("call_index"), -1)
            if not entry or call_index < 0:
                continue
            key = (entry, call_index)
            if key in roots:
                raise ValueError(f"duplicate structural root: {key}")
            roots[key] = row
    return roots


def main() -> int:
    parser = argparse.ArgumentParser(description="Plan factory-return probes for unresolved CALL-root receivers")
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("slice_dir", type=Path)
    parser.add_argument("output_tsv", type=Path)
    args = parser.parse_args()

    roots = load_slice_roots(args.slice_dir)
    plan: list[dict[str, Any]] = []
    for line_no, row in enumerate(read_jsonl(args.inference_jsonl), 1):
        if row.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"{args.inference_jsonl}:{line_no}: expected {INFERENCE_FORMAT}")
        if row.get("status") != "receiver_not_single_pointer":
            continue
        source_id = row.get("source_id")
        call_index = row.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            continue
        root = roots.get((source_entry(source_id), call_index))
        if root is None or root.get("defining_opcode") != "CALL":
            continue
        callee_entry = root.get("call_target_entry") or "-"
        callee_name = root.get("call_target_name") or "-"
        if callee_entry == "-" or callee_name == "-":
            continue
        plan.append({
            "source_id": source_id,
            "call_index": call_index,
            "source_entry": source_entry(source_id),
            "call_address": row.get("call_address") or "-",
            "dispatch_slot_offset": row.get("dispatch_slot_offset") or "-",
            "callee_entry": callee_entry,
            "callee_name": callee_name,
        })

    plan.sort(key=lambda r: (r["source_id"], r["call_index"]))
    args.output_tsv.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "source_id", "call_index", "source_entry", "call_address",
        "dispatch_slot_offset", "callee_entry", "callee_name",
    ]
    with args.output_tsv.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(plan)

    print("format: NEVERGONE.CallRootFactoryReturnPlan/1")
    print("CALL-root candidates:", len(plan))
    for row in plan:
        print(row["source_id"], "call", row["call_index"], "callee=", row["callee_entry"], row["callee_name"])
    print("output:", args.output_tsv)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
