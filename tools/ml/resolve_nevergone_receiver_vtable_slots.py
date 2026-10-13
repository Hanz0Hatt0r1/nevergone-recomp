#!/usr/bin/env python3
"""Resolve dispatch candidates by the exact receiver-vtable + slot key.

The resolver intentionally forbids cross-vtable and cross-class transfer. It joins
query rows that carry `receiver_vtable_id` and `dispatch_slot_offset` against the
conservative `NEVERGONE.StaticVtableSlotCatalog/3` evidence. Static-only matches
remain candidate evidence; only rows already marked `runtime_match` by the catalog
are reported as runtime-verified. The tool never changes production routing.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
OUTPUT_FORMAT = "NEVERGONE.ReceiverVtableSlotResolution/1"


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


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def normalize_slot(value: Any) -> str | None:
    if isinstance(value, int):
        if value < 0:
            return None
        return f"0x{value:x}"
    if not isinstance(value, str):
        return None
    try:
        parsed = int(value, 0)
    except ValueError:
        return None
    if parsed < 0:
        return None
    return f"0x{parsed:x}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("catalog", type=Path)
    parser.add_argument("queries", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    catalog_rows = read_jsonl(args.catalog)
    queries = read_jsonl(args.queries)

    index: dict[tuple[str, str], dict[str, Any]] = {}
    for line_no, row in enumerate(catalog_rows, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{args.catalog}:{line_no}: expected {CATALOG_FORMAT}")
        vtable_id = row.get("receiver_vtable_id")
        slot = normalize_slot(row.get("dispatch_slot_offset"))
        if not isinstance(vtable_id, str) or slot is None:
            raise ValueError(f"{args.catalog}:{line_no}: invalid exact receiver key")
        key = (vtable_id, slot)
        previous = index.get(key)
        if previous is not None:
            prev_targets = previous.get("candidate_target_ids") or []
            targets = row.get("candidate_target_ids") or []
            if prev_targets != targets:
                raise ValueError(f"duplicate exact key with conflicting targets: {vtable_id}@slot={slot}")
            continue
        index[key] = row

    output: list[dict[str, Any]] = []
    counts = {
        "runtime_verified": 0,
        "single_candidate": 0,
        "ambiguous": 0,
        "not_found": 0,
        "blocked_runtime_mismatch": 0,
        "invalid_query": 0,
    }

    for query_index, query in enumerate(queries):
        vtable_id = query.get("receiver_vtable_id")
        slot = normalize_slot(query.get("dispatch_slot_offset"))

        base = {
            "format": OUTPUT_FORMAT,
            "query_index": query_index,
            "source_id": query.get("source_id"),
            "call_index": query.get("call_index"),
            "call_address": query.get("call_address"),
            "receiver_vtable_id": vtable_id,
            "dispatch_slot_offset": slot,
            "receiver_dispatch_key": (
                f"{vtable_id}@slot={slot}" if isinstance(vtable_id, str) and slot is not None else None
            ),
            "query_evidence_format": query.get("format"),
            "query_evidence_ref": query.get("evidence_ref"),
            "target_transfer_scope": "same_receiver_vtable_and_slot_only",
            "cross_vtable_transfer_allowed": False,
            "cross_class_transfer_allowed": False,
            "affects_routing": False,
        }

        if not isinstance(vtable_id, str) or slot is None:
            base.update({
                "status": "invalid_query",
                "class_name": None,
                "candidate_target_ids": [],
                "candidate_target_count": 0,
                "exact_candidate_target_id": None,
                "catalog_runtime_validation_status": None,
                "runtime_verified": False,
            })
            counts["invalid_query"] += 1
            output.append(base)
            continue

        catalog = index.get((vtable_id, slot))
        if catalog is None:
            base.update({
                "status": "exact_key_not_found",
                "class_name": None,
                "candidate_target_ids": [],
                "candidate_target_count": 0,
                "exact_candidate_target_id": None,
                "catalog_runtime_validation_status": None,
                "runtime_verified": False,
            })
            counts["not_found"] += 1
            output.append(base)
            continue

        targets = catalog.get("candidate_target_ids") or []
        if not isinstance(targets, list) or not all(isinstance(x, str) for x in targets):
            raise ValueError(f"catalog key {vtable_id}@slot={slot}: invalid candidate_target_ids")
        targets = sorted(set(targets))
        runtime_status = catalog.get("runtime_validation_status")

        if runtime_status == "runtime_mismatch":
            status = "blocked_runtime_mismatch"
            exact_target = None
            verified = False
            counts["blocked_runtime_mismatch"] += 1
        elif len(targets) == 1 and runtime_status == "runtime_match":
            status = "exact_key_runtime_verified"
            exact_target = targets[0]
            verified = True
            counts["runtime_verified"] += 1
        elif len(targets) == 1:
            status = "exact_key_single_candidate"
            exact_target = targets[0]
            verified = False
            counts["single_candidate"] += 1
        else:
            status = "exact_key_ambiguous"
            exact_target = None
            verified = False
            counts["ambiguous"] += 1

        base.update({
            "status": status,
            "class_name": catalog.get("class_name"),
            "class_evidence_kind": catalog.get("class_evidence_kind"),
            "vtable_symbol_name": catalog.get("vtable_symbol_name"),
            "candidate_target_ids": targets,
            "candidate_target_count": len(targets),
            "exact_candidate_target_id": exact_target,
            "catalog_static_status": catalog.get("static_status"),
            "catalog_runtime_validation_status": runtime_status,
            "runtime_target_ids": catalog.get("runtime_target_ids") or [],
            "runtime_verified": verified,
            "primary_prefix_slot_count": catalog.get("primary_prefix_slot_count"),
            "catalog_source_kind": catalog.get("source_kind"),
        })
        output.append(base)

    write_jsonl(args.output, output)

    print("format:", OUTPUT_FORMAT)
    print("catalog exact keys:", len(index))
    print("queries:", len(queries))
    print("exact-key runtime verified:", counts["runtime_verified"])
    print("exact-key single candidates:", counts["single_candidate"])
    print("exact-key ambiguous:", counts["ambiguous"])
    print("exact-key not found:", counts["not_found"])
    print("blocked runtime mismatch:", counts["blocked_runtime_mismatch"])
    print("invalid queries:", counts["invalid_query"])
    print("cross-vtable transfer: disabled")
    print("affects routing: no")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
