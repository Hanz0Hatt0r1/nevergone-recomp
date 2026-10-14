#!/usr/bin/env python3
"""Audit per-call structural receiver slices without inferring a receiver class."""
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from plan_nevergone_slot80_receiver_slices import (
    FIELDS, PLAN_FORMAT, make_plan, selected_calls,
)

SLICE_FORMAT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
CAT_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
OUT_FORMAT = "NEVERGONE.Slot80ReceiverSliceAudit/1"


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def as_int(value: Any, default: int = -1) -> int:
    try:
        return int(str(value), 10)
    except (TypeError, ValueError):
        return default


def class_slots(catalog_path: Path, wanted: set[str]) -> dict[str, dict[str, Any]]:
    by_class: dict[str, dict[str, Any]] = defaultdict(lambda: {"vtables": set(), "slots": [], "prefix": set()})
    with catalog_path.open(encoding="utf-8") as handle:
        for line_no, line in enumerate(handle, 1):
            if not line.strip():
                continue
            row = json.loads(line)
            if row.get("format") != CAT_FORMAT:
                raise ValueError(f"{catalog_path}:{line_no}: catalog format mismatch")
            cls = row.get("class_name")
            if cls not in wanted:
                continue
            item = by_class[cls]
            item["vtables"].add(row.get("receiver_vtable_id"))
            item["prefix"].add(row.get("primary_prefix_slot_count"))
            if row.get("dispatch_slot_offset") == "0x80":
                item["slots"].append(row)
    result: dict[str, dict[str, Any]] = {}
    for cls in sorted(wanted):
        item = by_class.get(cls)
        if not item:
            result[cls] = {"status": "catalog_class_missing", "candidate_target_ids": []}
            continue
        vtables, prefixes, rows = item["vtables"], item["prefix"], item["slots"]
        if len(vtables) != 1 or len(prefixes) != 1 or not all(isinstance(x, str) for x in vtables):
            status = "catalog_vtable_or_prefix_not_unique"
        elif len(rows) == 1 and rows[0].get("runtime_validation_status") == "runtime_mismatch":
            status = "catalog_slot_runtime_mismatch"
        elif len(rows) == 1 and rows[0].get("candidate_target_count") == 1 and len(rows[0].get("candidate_target_ids") or []) == 1:
            status = "catalog_single_slot_target"
        elif not rows and isinstance(next(iter(prefixes)), int) and next(iter(prefixes)) * 4 <= 0x80:
            status = "slot_outside_observed_primary_prefix"
        elif not rows:
            status = "slot_missing_or_catalog_incomplete"
        else:
            status = "catalog_slot_not_unique"
        result[cls] = {
            "status": status,
            "receiver_vtable_ids": sorted(vtables),
            "primary_prefix_slot_counts": sorted(prefixes),
            "candidate_target_ids": rows[0].get("candidate_target_ids", []) if len(rows) == 1 else [],
            "runtime_validation_status": rows[0].get("runtime_validation_status") if len(rows) == 1 else None,
        }
    return result


def audit_call(source: str, failure: dict[str, Any], rows: list[dict[str, str]] | None,
               slot_evidence: dict[str, dict[str, Any]]) -> dict[str, Any]:
    call = failure["call_index"]
    reasons: list[str] = []
    if rows is None:
        reasons.append("slice_file_missing")
        matched: list[dict[str, str]] = []
    else:
        matched = [r for r in rows if as_int(r.get("call_index")) == call]
    summaries = [r for r in matched if r.get("row_type") == "summary"]
    nodes = [r for r in matched if r.get("row_type") == "node"]
    summary = summaries[0] if len(summaries) == 1 else None
    if len(summaries) != 1:
        reasons.append("slice_summary_not_unique")
    if summary is not None:
        if (summary.get("function_entry") != source.split(":", 1)[1]
                or summary.get("call_address") != failure["call_address"]
                or summary.get("slot_offset") != "0x80"
                or summary.get("recovery_status") != "canonical_receiver"):
            reasons.append("slice_call_identity_mismatch")
        if (as_int(summary.get("truncated_by_budget"), 1) != 0
                or as_int(summary.get("depth_truncation_count"), 1) != 0
                or as_int(summary.get("cycle_count"), 1) != 0):
            reasons.append("slice_truncated_or_cyclic")
        if as_int(summary.get("node_count")) != len(nodes):
            reasons.append("slice_node_count_mismatch")
    ids = [as_int(r.get("node_id")) for r in nodes]
    if not nodes or len(ids) != len(set(ids)) or ids.count(0) != 1 or any(i < 0 for i in ids):
        reasons.append("slice_node_ids_invalid")
    if any(r.get("truncation") not in (None, "", "-") for r in nodes):
        reasons.append("slice_node_truncated")
    if any(r.get("function_entry") != source.split(":", 1)[1]
           or r.get("call_address") != failure["call_address"]
           or r.get("slot_offset") != "0x80" for r in nodes):
        reasons.append("slice_node_identity_mismatch")
    root = next((r for r in nodes if as_int(r.get("node_id")) == 0), None)
    if root and as_int(root.get("parent_node_id")) != -1:
        reasons.append("slice_root_parent_invalid")
    if any(as_int(r.get("parent_node_id")) not in ids and as_int(r.get("node_id")) != 0 for r in nodes):
        reasons.append("slice_parent_missing")
    calls = sorted({(r.get("call_target_entry"), r.get("call_target_name")) for r in nodes
                    if r.get("defining_opcode") == "CALL" and r.get("call_target_entry") not in (None, "", "-")})
    candidate_classes = failure.get("candidate_receiver_class_names") or []
    return {
        "format": OUT_FORMAT,
        "source_id": source,
        "call_index": call,
        "call_address": failure["call_address"],
        "dispatch_slot_offset": "0x80",
        "previous_status": failure["status"],
        "root_opcode": root.get("defining_opcode") if root else None,
        "root_token": root.get("node_token") if root else None,
        "node_count": len(nodes),
        "call_roots": [{"entry": entry, "name": name} for entry, name in calls],
        "candidate_class_slot_evidence": {cls: slot_evidence[cls] for cls in candidate_classes},
        "status": "complete_structural_slice" if not reasons else "structural_slice_not_proven_complete",
        "reasons": reasons,
        "affects_routing": False,
    }


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("plan_tsv", type=Path)
    p.add_argument("failures_jsonl", type=Path)
    p.add_argument("inference_jsonl", type=Path)
    p.add_argument("catalog_jsonl", type=Path)
    p.add_argument("slice_dir", type=Path)
    p.add_argument("--details-output", required=True, type=Path)
    p.add_argument("--require-complete", action="store_true")
    a = p.parse_args()
    failures = selected_calls(a.failures_jsonl, a.inference_jsonl)
    expected = make_plan(failures)
    plan = read_tsv(a.plan_tsv)
    if plan != expected or any(set(row) != set(FIELDS) or row.get("format") != PLAN_FORMAT for row in plan):
        raise ValueError("slot80 plan differs from exact current v11 failures")
    wanted = {cls for group in failures.values() for r in group
              for cls in r.get("candidate_receiver_class_names", [])}
    evidence = class_slots(a.catalog_jsonl, wanted)
    details: list[dict[str, Any]] = []
    for source, group in sorted(failures.items()):
        path = a.slice_dir / (source.replace(":", "_").replace("/", "_") + ".tsv")
        rows = read_tsv(path) if path.is_file() else None
        if rows is not None and any(row.get("format") != SLICE_FORMAT for row in rows):
            raise ValueError(f"{path}: structural slice format mismatch")
        for failure in sorted(group, key=lambda r: r["call_index"]):
            details.append(audit_call(source, failure, rows, evidence))
    a.details_output.parent.mkdir(parents=True, exist_ok=True)
    with a.details_output.open("w", encoding="utf-8") as handle:
        for row in details:
            handle.write(json.dumps(row, sort_keys=True) + "\n")
    counts = Counter(row["status"] for row in details)
    print("format:", OUT_FORMAT)
    print("audited calls:", len(details))
    print("status counts:", dict(counts))
    for row in details:
        print(row["source_id"], "call", row["call_index"], "root", row["root_opcode"],
              "status", row["status"], "calls", row["call_roots"],
              "candidate slot evidence", {k: v["status"] for k, v in row["candidate_class_slot_evidence"].items()},
              "reasons", row["reasons"])
    print("details output:", a.details_output)
    print("affects routing: no")
    return 2 if a.require_complete and counts["complete_structural_slice"] != len(details) else 0


if __name__ == "__main__":
    raise SystemExit(main())
