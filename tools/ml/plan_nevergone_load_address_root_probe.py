#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import re
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


def main() -> int:
    parser = argparse.ArgumentParser(description="Plan evidence-only classification of unresolved LOAD address roots")
    parser.add_argument("failure_jsonl", type=Path)
    parser.add_argument("slice_dir", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    out: list[dict[str, Any]] = []
    for failure in read_jsonl(args.failure_jsonl):
        if failure.get("status") != "receiver_not_single_pointer":
            continue
        source_id = failure.get("source_id")
        call_index = failure.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            continue
        path = args.slice_dir / f"{safe_id(source_id)}.tsv"
        if not path.is_file():
            continue
        rows = read_tsv(path)
        for line_no, row in enumerate(rows, 2):
            if row.get("format") != SLICE_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {SLICE_FORMAT}")
        call_rows = [r for r in rows if dec(r.get("call_index")) == call_index]
        summary = next((r for r in call_rows if r.get("row_type") == "summary"), None)
        nodes = [r for r in call_rows if r.get("row_type") == "node"]
        root = next((r for r in nodes if dec(r.get("node_id")) == 0), None)
        if summary is None or root is None or root.get("defining_opcode") != "LOAD":
            continue
        children = [r for r in nodes if dec(r.get("parent_node_id")) == 0]
        address_children = []
        for child in children:
            address = address_from_token(child.get("node_token"))
            if address is not None and child.get("defining_opcode") in {None, "", "-"}:
                address_children.append((child, address))
        if len(address_children) != 1:
            continue
        child, root_address = address_children[0]
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
            "address_node_id": dec(child.get("node_id")),
            "address_token": child.get("node_token"),
            "root_address": root_address,
            "affects_routing": False,
        })

    args.output.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "format", "source_id", "call_index", "call_address", "dispatch_slot_offset",
        "function_entry", "recovery_status", "root_opcode", "root_token",
        "address_node_id", "address_token", "root_address", "affects_routing",
    ]
    with args.output.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(out)

    print("format:", PLAN_FORMAT)
    print("planned LOAD address roots:", len(out))
    for row in out:
        print(row["source_id"], "call", row["call_index"], "slot=", row["dispatch_slot_offset"], "root_address=", row["root_address"])
    print("output:", args.output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
