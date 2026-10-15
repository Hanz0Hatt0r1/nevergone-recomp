#!/usr/bin/env python3
"""Audit bounded structural slices for the post-v17 non-slot-0x80 frontier.

This stage classifies receiver *provenance shape* only. A complete slice can select
which proof family should run next (LOAD write provenance, CALL return provenance,
PHI inputs, or a generic structural follow-up), but it never assigns a receiver
class, vtable, target, or routing decision.
"""
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
from pathlib import Path
from typing import Any

from plan_nevergone_non_slot80_receiver_frontier_v18 import (
    FIELDS,
    PLAN_FORMAT,
    make_plan,
    selected_calls,
)

SLICE_FORMAT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
OUT_FORMAT = "NEVERGONE.NonSlot80ReceiverFrontierAudit/1"


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def as_int(value: Any, default: int = -1) -> int:
    try:
        return int(str(value), 10)
    except (TypeError, ValueError):
        return default


def next_probe(root_opcode: str | None) -> str:
    if root_opcode == "LOAD":
        return "receiver_load_write_provenance"
    if root_opcode == "CALL":
        return "receiver_call_return_provenance"
    if root_opcode == "MULTIEQUAL":
        return "receiver_phi_input_provenance"
    if root_opcode in {"PTRSUB", "PTRADD", "INT_ADD", "INT_SUB"}:
        return "receiver_address_expression_provenance"
    if root_opcode in {None, "", "-"}:
        return "receiver_input_provenance"
    return "receiver_structural_provenance"


def audit_call(
    source: str,
    failure: dict[str, Any],
    rows: list[dict[str, str]] | None,
) -> dict[str, Any]:
    call = failure["call_index"]
    slot = failure["dispatch_slot_offset"]
    reasons: list[str] = []

    if rows is None:
        matched: list[dict[str, str]] = []
        reasons.append("slice_file_missing")
    else:
        matched = [row for row in rows if as_int(row.get("call_index")) == call]

    summaries = [row for row in matched if row.get("row_type") == "summary"]
    nodes = [row for row in matched if row.get("row_type") == "node"]
    summary = summaries[0] if len(summaries) == 1 else None
    if len(summaries) != 1:
        reasons.append("slice_summary_not_unique")

    source_entry = source.split(":", 1)[1]
    if summary is not None:
        if (
            summary.get("function_entry") != source_entry
            or summary.get("call_address") != failure["call_address"]
            or summary.get("slot_offset") != slot
            or summary.get("recovery_status") != "canonical_receiver"
        ):
            reasons.append("slice_call_identity_mismatch")
        if (
            as_int(summary.get("truncated_by_budget"), 1) != 0
            or as_int(summary.get("depth_truncation_count"), 1) != 0
            or as_int(summary.get("cycle_count"), 1) != 0
        ):
            reasons.append("slice_truncated_or_cyclic")
        if as_int(summary.get("node_count")) != len(nodes):
            reasons.append("slice_node_count_mismatch")

    node_ids = [as_int(row.get("node_id")) for row in nodes]
    if (
        not nodes
        or len(node_ids) != len(set(node_ids))
        or node_ids.count(0) != 1
        or any(node_id < 0 for node_id in node_ids)
    ):
        reasons.append("slice_node_ids_invalid")
    if any(row.get("truncation") not in (None, "", "-") for row in nodes):
        reasons.append("slice_node_truncated")
    if any(
        row.get("function_entry") != source_entry
        or row.get("call_address") != failure["call_address"]
        or row.get("slot_offset") != slot
        for row in nodes
    ):
        reasons.append("slice_node_identity_mismatch")

    root = next((row for row in nodes if as_int(row.get("node_id")) == 0), None)
    if root is not None and as_int(root.get("parent_node_id")) != -1:
        reasons.append("slice_root_parent_invalid")
    if any(
        as_int(row.get("parent_node_id")) not in node_ids
        and as_int(row.get("node_id")) != 0
        for row in nodes
    ):
        reasons.append("slice_parent_missing")

    opcodes = Counter(
        row.get("defining_opcode") or "-"
        for row in nodes
    )
    constants = sorted(
        {
            value
            for row in nodes
            for value in [row.get("constant_value")]
            if value not in (None, "", "-")
        }
    )
    call_roots = sorted(
        {
            (
                row.get("call_target_entry"),
                row.get("call_target_name"),
                row.get("call_target_return_type"),
            )
            for row in nodes
            if row.get("defining_opcode") == "CALL"
            and row.get("call_target_entry") not in (None, "", "-")
        }
    )
    root_opcode = root.get("defining_opcode") if root else None
    probe = next_probe(root_opcode)

    return {
        "format": OUT_FORMAT,
        "source_id": source,
        "call_index": call,
        "call_address": failure["call_address"],
        "dispatch_slot_offset": slot,
        "previous_status": failure["status"],
        "receiver_storage_kind": failure.get("receiver_storage_kind"),
        "receiver_token": failure.get("receiver_token"),
        "receiver_def_mnemonic": failure.get("receiver_def_mnemonic"),
        "receiver_type_display": failure.get("receiver_type_display"),
        "receiver_pointer_depth": failure.get("receiver_pointer_depth"),
        "pointee_type_display": failure.get("pointee_type_display"),
        "flow_type_node_count": failure.get("flow_type_node_count"),
        "root_opcode": root_opcode,
        "root_token": root.get("node_token") if root else None,
        "root_type_display": root.get("type_display") if root else None,
        "root_pointer_depth": as_int(root.get("pointer_depth"), 0) if root else 0,
        "root_pointee_type_display": root.get("pointee_type_display") if root else None,
        "node_count": len(nodes),
        "node_opcode_counts": dict(sorted(opcodes.items())),
        "constant_values": constants,
        "call_roots": [
            {"entry": entry, "name": name, "return_type": return_type}
            for entry, name, return_type in call_roots
        ],
        "next_probe_kind": probe,
        "status": (
            "complete_structural_slice"
            if not reasons
            else "structural_slice_not_proven_complete"
        ),
        "reasons": reasons,
        "receiver_class_proven": False,
        "affects_routing": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("plan_tsv", type=Path)
    parser.add_argument("failures_jsonl", type=Path)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("slice_dir", type=Path)
    parser.add_argument("--details-output", required=True, type=Path)
    parser.add_argument("--require-complete", action="store_true")
    args = parser.parse_args()

    failures = selected_calls(args.failures_jsonl, args.inference_jsonl)
    expected_plan = make_plan(failures)
    actual_plan = read_tsv(args.plan_tsv)
    if actual_plan != expected_plan:
        raise ValueError("v18 plan differs from exact current post-v17 failure frontier")
    if any(
        set(row) != set(FIELDS) or row.get("format") != PLAN_FORMAT
        for row in actual_plan
    ):
        raise ValueError("v18 plan schema mismatch")

    details: list[dict[str, Any]] = []
    for source, group in sorted(failures.items()):
        safe = source.replace(":", "_").replace("/", "_")
        path = args.slice_dir / f"{safe}.tsv"
        rows = read_tsv(path) if path.is_file() else None
        if rows is not None and any(row.get("format") != SLICE_FORMAT for row in rows):
            raise ValueError(f"{path}: structural slice format mismatch")
        for failure in sorted(group, key=lambda row: row["call_index"]):
            details.append(audit_call(source, failure, rows))

    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    with args.details_output.open("w", encoding="utf-8") as handle:
        for row in details:
            handle.write(json.dumps(row, sort_keys=True) + "\n")

    statuses = Counter(row["status"] for row in details)
    slots = Counter(row["dispatch_slot_offset"] for row in details)
    roots = Counter(row["root_opcode"] or "<missing>" for row in details)
    probes = Counter(row["next_probe_kind"] for row in details)

    print("format:", OUT_FORMAT)
    print("audited calls:", len(details))
    print("status counts:", dict(statuses))
    print("slot counts:", dict(sorted(slots.items())))
    print("root opcode counts:", dict(roots.most_common()))
    print("next probe counts:", dict(probes.most_common()))
    for row in details:
        print(
            row["source_id"],
            "call", row["call_index"],
            "slot", row["dispatch_slot_offset"],
            "root", row["root_opcode"],
            "next", row["next_probe_kind"],
            "status", row["status"],
            "reasons", row["reasons"],
        )
    print("details output:", args.details_output)
    print("receiver class proven: no")
    print("affects routing: no")

    if args.require_complete and statuses["complete_structural_slice"] != len(details):
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
