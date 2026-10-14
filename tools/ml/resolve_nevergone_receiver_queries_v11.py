#!/usr/bin/env python3
"""Resolve legacy exact-vtable queries plus audited multi-vtable target convergence.

Legacy queries retain the exact receiver-vtable+slot semantics. A convergence
query is accepted only when it has no singular receiver_vtable_id, carries at
least two distinct candidate receiver vtables, and every candidate exact
vtable+slot catalog row independently resolves to the same declared target.
Thus the new mode does not select one receiver vtable and does not permit an
unchecked cross-vtable target transfer.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
QUERY_FORMAT = "NEVERGONE.StaticReceiverVtableQuery/1"
OUTPUT_FORMAT = "NEVERGONE.ReceiverQueryResolutionV11/1"
MODE = "audited_multi_vtable_target_convergence"


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
        return f"0x{value:x}" if value >= 0 else None
    if not isinstance(value, str):
        return None
    try:
        parsed = int(value, 0)
    except ValueError:
        return None
    return f"0x{parsed:x}" if parsed >= 0 else None


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("catalog", type=Path)
    p.add_argument("queries", type=Path)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()

    catalog_rows = read_jsonl(a.catalog)
    queries = read_jsonl(a.queries)
    index: dict[tuple[str, str], dict[str, Any]] = {}
    for line_no, row in enumerate(catalog_rows, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{a.catalog}:{line_no}: expected {CATALOG_FORMAT}")
        vt = row.get("receiver_vtable_id")
        slot = normalize_slot(row.get("dispatch_slot_offset"))
        if not isinstance(vt, str) or slot is None:
            raise ValueError(f"{a.catalog}:{line_no}: invalid exact receiver key")
        key = (vt, slot)
        if key in index and index[key].get("candidate_target_ids") != row.get("candidate_target_ids"):
            raise ValueError(f"duplicate exact key with conflicting targets: {vt}@slot={slot}")
        index[key] = row

    out: list[dict[str, Any]] = []
    counts = {
        "exact_runtime_verified": 0,
        "exact_single_candidate": 0,
        "exact_ambiguous": 0,
        "exact_not_found": 0,
        "runtime_mismatch": 0,
        "invalid_query": 0,
        "converged": 0,
        "convergence_blocked": 0,
    }

    for query_index, query in enumerate(queries):
        if query.get("format") != QUERY_FORMAT:
            raise ValueError(f"{a.queries}:{query_index+1}: expected {QUERY_FORMAT}")
        slot = normalize_slot(query.get("dispatch_slot_offset"))
        mode = query.get("resolution_mode")
        base = {
            "format": OUTPUT_FORMAT,
            "query_index": query_index,
            "source_id": query.get("source_id"),
            "call_index": query.get("call_index"),
            "call_address": query.get("call_address"),
            "dispatch_slot_offset": slot,
            "query_evidence_format": query.get("format"),
            "query_evidence_ref": query.get("evidence_ref"),
            "resolution_mode": mode or "exact_receiver_vtable_and_slot",
            "affects_routing": False,
        }

        if mode == MODE:
            reasons: list[str] = []
            if query.get("receiver_vtable_id") not in (None, ""):
                reasons.append("singular_receiver_vtable_must_be_null")
            vtables = query.get("candidate_receiver_vtable_ids")
            declared_target = query.get("converged_target_id")
            if not isinstance(vtables, list) or len(vtables) < 2 or not all(isinstance(v, str) and v for v in vtables):
                reasons.append("candidate_vtable_set_invalid")
                normalized_vtables: list[str] = []
            else:
                normalized_vtables = sorted(set(vtables))
                if len(normalized_vtables) != len(vtables):
                    reasons.append("candidate_vtables_not_unique")
            if slot is None:
                reasons.append("slot_invalid")
            if not isinstance(declared_target, str) or not declared_target:
                reasons.append("converged_target_invalid")

            catalog_rows_used: list[dict[str, Any]] = []
            runtime_statuses: list[str] = []
            if not reasons:
                for vt in normalized_vtables:
                    row = index.get((vt, slot))
                    if row is None:
                        reasons.append(f"catalog_key_missing:{vt}")
                        continue
                    targets = row.get("candidate_target_ids")
                    if row.get("static_status") != "single_function_memory_pointer":
                        reasons.append(f"catalog_not_single_function:{vt}")
                    if not isinstance(targets, list) or targets != [declared_target]:
                        reasons.append(f"catalog_target_mismatch:{vt}")
                    runtime = row.get("runtime_validation_status")
                    if runtime == "runtime_mismatch":
                        reasons.append(f"runtime_mismatch:{vt}")
                    runtime_statuses.append(str(runtime))
                    catalog_rows_used.append({
                        "receiver_vtable_id": vt,
                        "class_name": row.get("class_name"),
                        "catalog_static_status": row.get("static_status"),
                        "catalog_runtime_validation_status": runtime,
                        "candidate_target_ids": targets if isinstance(targets, list) else [],
                    })

            if reasons:
                base.update({
                    "status": "blocked_audited_multi_vtable_target_convergence",
                    "candidate_receiver_vtable_ids": normalized_vtables,
                    "candidate_target_ids": [],
                    "candidate_target_count": 0,
                    "exact_candidate_target_id": None,
                    "catalog_rows": catalog_rows_used,
                    "reasons": reasons,
                    "runtime_verified": False,
                    "target_transfer_scope": "audited_candidate_vtables_same_slot_only",
                    "cross_vtable_transfer_allowed": False,
                    "cross_class_transfer_allowed": False,
                })
                counts["convergence_blocked"] += 1
            else:
                verified = bool(runtime_statuses) and all(x == "runtime_match" for x in runtime_statuses)
                base.update({
                    "status": "audited_multi_vtable_target_convergence_runtime_verified" if verified else "audited_multi_vtable_target_convergence",
                    "candidate_receiver_vtable_ids": normalized_vtables,
                    "candidate_target_ids": [declared_target],
                    "candidate_target_count": 1,
                    "exact_candidate_target_id": declared_target,
                    "catalog_rows": catalog_rows_used,
                    "reasons": [],
                    "runtime_verified": verified,
                    "target_transfer_scope": "all_audited_candidate_vtables_same_slot_converge",
                    "cross_vtable_transfer_allowed": False,
                    "cross_class_transfer_allowed": False,
                })
                counts["converged"] += 1
            out.append(base)
            continue

        vt = query.get("receiver_vtable_id")
        if not isinstance(vt, str) or slot is None:
            base.update({
                "status": "invalid_query",
                "receiver_vtable_id": vt,
                "candidate_target_ids": [],
                "candidate_target_count": 0,
                "exact_candidate_target_id": None,
                "runtime_verified": False,
                "target_transfer_scope": "same_receiver_vtable_and_slot_only",
                "cross_vtable_transfer_allowed": False,
                "cross_class_transfer_allowed": False,
            })
            counts["invalid_query"] += 1
            out.append(base)
            continue

        row = index.get((vt, slot))
        if row is None:
            base.update({
                "status": "exact_key_not_found",
                "receiver_vtable_id": vt,
                "candidate_target_ids": [],
                "candidate_target_count": 0,
                "exact_candidate_target_id": None,
                "runtime_verified": False,
                "target_transfer_scope": "same_receiver_vtable_and_slot_only",
                "cross_vtable_transfer_allowed": False,
                "cross_class_transfer_allowed": False,
            })
            counts["exact_not_found"] += 1
            out.append(base)
            continue

        targets = row.get("candidate_target_ids") or []
        if not isinstance(targets, list) or not all(isinstance(x, str) for x in targets):
            raise ValueError(f"catalog key {vt}@slot={slot}: invalid candidate_target_ids")
        targets = sorted(set(targets))
        runtime = row.get("runtime_validation_status")
        if runtime == "runtime_mismatch":
            status, exact, verified = "blocked_runtime_mismatch", None, False
            counts["runtime_mismatch"] += 1
        elif len(targets) == 1 and runtime == "runtime_match":
            status, exact, verified = "exact_key_runtime_verified", targets[0], True
            counts["exact_runtime_verified"] += 1
        elif len(targets) == 1:
            status, exact, verified = "exact_key_single_candidate", targets[0], False
            counts["exact_single_candidate"] += 1
        else:
            status, exact, verified = "exact_key_ambiguous", None, False
            counts["exact_ambiguous"] += 1
        base.update({
            "status": status,
            "receiver_vtable_id": vt,
            "class_name": row.get("class_name"),
            "candidate_target_ids": targets,
            "candidate_target_count": len(targets),
            "exact_candidate_target_id": exact,
            "catalog_static_status": row.get("static_status"),
            "catalog_runtime_validation_status": runtime,
            "runtime_verified": verified,
            "target_transfer_scope": "same_receiver_vtable_and_slot_only",
            "cross_vtable_transfer_allowed": False,
            "cross_class_transfer_allowed": False,
        })
        out.append(base)

    write_jsonl(a.output, out)
    print("format:", OUTPUT_FORMAT)
    print("catalog exact keys:", len(index))
    print("queries:", len(queries))
    print("exact-key runtime verified:", counts["exact_runtime_verified"])
    print("exact-key single candidates:", counts["exact_single_candidate"])
    print("exact-key ambiguous:", counts["exact_ambiguous"])
    print("exact-key not found:", counts["exact_not_found"])
    print("blocked runtime mismatch:", counts["runtime_mismatch"])
    print("invalid queries:", counts["invalid_query"])
    print("audited multi-vtable converged:", counts["converged"])
    print("audited multi-vtable blocked:", counts["convergence_blocked"])
    print("affects routing: no")
    print("output:", a.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
