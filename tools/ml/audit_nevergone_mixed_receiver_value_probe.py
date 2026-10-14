#!/usr/bin/env python3
"""Audit the actual CALLIND receiver values for the three quarantined v11 calls.

This intentionally never infers receiver classes from caller arguments, function
names or catalog slot convergence. A complete receiver slice is still not a
receiver-class proof.
"""
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
from pathlib import Path
from typing import Any

SOURCE = "libcocos2dcpp.so:002a7594"
ENTRY = "002a7594"
CALLS = (3, 4, 7)
SLOT = "0x80"
INF_FMT = "NEVERGONE.StaticReceiverVtableInference/1"
SLICE_FMT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
OUT_FMT = "NEVERGONE.MixedVirtualReceiverValueSliceAudit/1"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    with path.open(encoding="utf-8") as handle:
        rows = [json.loads(line) for line in handle if line.strip()]
    if any(not isinstance(row, dict) for row in rows):
        raise ValueError("expected JSON objects")
    return rows


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def number(value: Any, default: int = -1) -> int:
    try:
        return int(str(value), 10)
    except (ValueError, TypeError):
        return default


def select_inference(rows: list[dict[str, Any]]) -> dict[int, dict[str, Any]]:
    selected: dict[int, dict[str, Any]] = {}
    for row in rows:
        if row.get("format") != INF_FMT:
            raise ValueError("inference format mismatch")
        if row.get("source_id") != SOURCE or row.get("call_index") not in CALLS:
            continue
        call = row["call_index"]
        if call in selected:
            raise ValueError(f"duplicate inference call {call}")
        if (row.get("status") != "receiver_not_single_pointer"
                or row.get("dispatch_slot_offset") != SLOT
                or not isinstance(row.get("call_address"), str)
                or len(row["call_address"]) != 8):
            raise ValueError(f"rerun v11 with quarantined caller-argument fix: call {call}")
        selected[call] = row
    if set(selected) != set(CALLS):
        raise ValueError(f"expected exactly three quarantined v11 calls, found {sorted(selected)}")
    return selected


def audit_one(call: int, inference: dict[str, Any], rows: list[dict[str, str]]) -> dict[str, Any]:
    group = [r for r in rows if number(r.get("call_index")) == call]
    summaries = [r for r in group if r.get("row_type") == "summary"]
    nodes = [r for r in group if r.get("row_type") == "node"]
    reasons: list[str] = []
    if len(summaries) != 1:
        reasons.append("summary_not_unique")
    summary = summaries[0] if len(summaries) == 1 else {}
    for row in group:
        if (row.get("format") != SLICE_FMT
                or row.get("function_entry") != ENTRY
                or row.get("call_address") != inference["call_address"]
                or row.get("slot_offset") != SLOT):
            reasons.append("callsite_identity_mismatch")
            break
    if not group:
        reasons.append("callsite_missing")
    if summary.get("recovery_status") != "canonical_receiver":
        reasons.append("noncanonical_receiver")
    if (number(summary.get("node_count")) != len(nodes)
            or number(summary.get("truncated_by_budget"), 1) != 0
            or number(summary.get("depth_truncation_count"), 1) != 0
            or number(summary.get("cycle_count"), 1) != 0):
        reasons.append("incomplete_receiver_slice")
    ids = [number(row.get("node_id")) for row in nodes]
    if not ids or sorted(ids) != list(range(len(nodes))):
        reasons.append("invalid_node_ids")
    by_id = {number(row.get("node_id")): row for row in nodes}
    root = by_id.get(0, {})
    if (number(root.get("parent_node_id")) != -1
            or root.get("node_token") != summary.get("structural_receiver_token")):
        reasons.append("root_identity_mismatch")
    for node in nodes:
        node_id = number(node.get("node_id"))
        parent = number(node.get("parent_node_id"))
        if node.get("truncation") not in (None, "", "-"):
            reasons.append("node_truncated")
        if node_id != 0 and (parent not in by_id or parent >= node_id):
            reasons.append("invalid_parent_edge")
    constants = sorted({
        row["constant_value"] for row in nodes
        if row.get("constant_value") not in (None, "", "-")
    })
    call_roots = sorted({
        (row.get("call_target_entry"), row.get("call_target_name"))
        for row in nodes if row.get("defining_opcode") == "CALL"
        and row.get("call_target_entry") not in (None, "", "-")
    })
    return {
        "format": OUT_FMT, "source_id": SOURCE, "call_index": call,
        "call_address": inference["call_address"], "dispatch_slot_offset": SLOT,
        "receiver_provenance_kind": "direct_callind_receiver_slice",
        "root_opcode": root.get("defining_opcode"),
        "root_type_display": root.get("type_display"),
        "root_token": root.get("node_token"), "node_count": len(nodes),
        "node_opcodes": [row.get("defining_opcode") for _, row in sorted(by_id.items())],
        "constants": constants,
        "call_roots": [{"entry": entry, "name": name} for entry, name in call_roots],
        "status": "complete_virtual_receiver_slice" if not reasons else "receiver_slice_incomplete",
        "reasons": sorted(set(reasons)),
        "receiver_class_proven": False, "affects_routing": False,
    }


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("v11_inference_jsonl", type=Path)
    p.add_argument("ghidra_receiver_slices_tsv", type=Path)
    p.add_argument("--details-output", type=Path, required=True)
    p.add_argument("--require-complete", action="store_true")
    args = p.parse_args()
    inf = select_inference(read_jsonl(args.v11_inference_jsonl))
    slices = read_tsv(args.ghidra_receiver_slices_tsv)
    if any(row.get("format") != SLICE_FMT for row in slices):
        raise ValueError("Ghidra slice format mismatch")
    if any(number(row.get("call_index")) in CALLS and row.get("row_type") not in {"summary", "node"}
           for row in slices):
        raise ValueError("unexpected target call row type")
    results = [audit_one(call, inf[call], slices) for call in CALLS]
    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    with args.details_output.open("w", encoding="utf-8") as handle:
        for row in results:
            handle.write(json.dumps(row, sort_keys=True) + "\n")
    counts = Counter(row["status"] for row in results)
    print("format:", OUT_FMT)
    print("call counts:", dict(counts))
    for row in results:
        print("call", row["call_index"], "root", row["root_opcode"], "nodes", row["node_count"],
              "constants", row["constants"], "calls", row["call_roots"], "reasons", row["reasons"])
    print("receiver class proven: no; affects routing: no")
    print("output:", args.details_output)
    return 2 if args.require_complete and counts["complete_virtual_receiver_slice"] != 3 else 0


if __name__ == "__main__":
    raise SystemExit(main())
