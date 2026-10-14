#!/usr/bin/env python3
"""Resolve two v11 missing receivers from exact singleton-cell provenance.

This post-finalizer is intentionally narrow. It accepts only the two slot-0x8
rows whose receiver LOAD effective addresses were independently folded to exact
writable .bss singleton cells. A recovery requires an accepted
ExactSingletonCellProvenanceAudit row proving:

  constructor(this=root) -> STORE root to exact cell -> LOAD exact cell -> RETURN

The finalizer then independently re-checks the unique class/vtable/slot/target
against StaticVtableSlotCatalog/3 before emitting a normal exact-vtable query.
Accessor names or nearby-class hints are never used as receiver-class evidence.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
QUERY_FORMAT = "NEVERGONE.StaticReceiverVtableQuery/1"
AUDIT_FORMAT = "NEVERGONE.ExactSingletonCellProvenanceAudit/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
RECOVERY_FORMAT = "NEVERGONE.ExactSingletonCellRecovery/1"
FINAL_FORMAT = "NEVERGONE.ExactSingletonCellRecoveryFinalization/1"
RESOLVED = {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match", "resolved_converged_target"}

EXPECTED = {
    ("libcocos2dcpp.so:0029edb4", 0): {
        "cell": "0094cb44",
        "accessor": "ActionDataManager::sharedActionDataManager",
        "slot": "0x8",
    },
    ("libcocos2dcpp.so:002a08fc", 1): {
        "cell": "0094cbd8",
        "accessor": "AndroidSdkMgr::getInstance",
        "slot": "0x8",
    },
}


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


def key_of(row: dict[str, Any], where: str) -> tuple[str, int]:
    source = row.get("source_id")
    call = row.get("call_index")
    if not isinstance(source, str) or not source or not isinstance(call, int) or call < 0:
        raise ValueError(f"{where}: invalid source_id/call_index")
    return source, call


def normalize_slot(value: Any) -> str | None:
    if isinstance(value, int):
        return f"0x{value:x}" if value >= 0 else None
    if not isinstance(value, str) or not value:
        return None
    try:
        parsed = int(value, 0)
    except ValueError:
        return None
    return f"0x{parsed:x}" if parsed >= 0 else None


def normalize_address(value: Any) -> str | None:
    if value in (None, "", "-"):
        return None
    text = str(value).strip().lower()
    if text.startswith("0x"):
        text = text[2:]
    try:
        return f"{int(text, 16) & 0xffffffff:08x}"
    except ValueError:
        return None


def unique_string_list(value: Any) -> list[str] | None:
    if not isinstance(value, list) or len(value) != 1 or not isinstance(value[0], str) or not value[0]:
        return None
    return [value[0]]


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("v11_inference_jsonl", type=Path)
    p.add_argument("v11_queries_jsonl", type=Path)
    p.add_argument("singleton_audit_jsonl", type=Path)
    p.add_argument("catalog_jsonl", type=Path)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--queries-output", type=Path, required=True)
    p.add_argument("--details-output", type=Path)
    a = p.parse_args()

    inference = read_jsonl(a.v11_inference_jsonl)
    queries = read_jsonl(a.v11_queries_jsonl)
    audit_rows = read_jsonl(a.singleton_audit_jsonl)
    catalog = read_jsonl(a.catalog_jsonl)

    audits: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, row in enumerate(audit_rows, 1):
        if row.get("format") != AUDIT_FORMAT:
            raise ValueError(f"{a.singleton_audit_jsonl}:{line_no}: expected {AUDIT_FORMAT}")
        key = key_of(row, f"{a.singleton_audit_jsonl}:{line_no}")
        if key not in EXPECTED:
            raise ValueError(f"unexpected singleton recovery audit row: {key}")
        if key in audits:
            raise ValueError(f"duplicate singleton recovery audit row: {key}")
        audits[key] = row
    if set(audits) != set(EXPECTED):
        raise ValueError(f"singleton recovery audit key set changed: {sorted(audits)}")

    catalog_index: dict[tuple[str, str], list[dict[str, Any]]] = {}
    for line_no, row in enumerate(catalog, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{a.catalog_jsonl}:{line_no}: expected {CATALOG_FORMAT}")
        vt = row.get("receiver_vtable_id")
        slot = normalize_slot(row.get("dispatch_slot_offset"))
        if isinstance(vt, str) and vt and slot is not None:
            catalog_index.setdefault((vt, slot), []).append(row)

    query_by_key: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, query in enumerate(queries, 1):
        if query.get("format") != QUERY_FORMAT:
            raise ValueError(f"{a.v11_queries_jsonl}:{line_no}: expected {QUERY_FORMAT}")
        key = key_of(query, f"{a.v11_queries_jsonl}:{line_no}")
        if key in query_by_key:
            raise ValueError(f"duplicate query: {key}")
        query_by_key[key] = dict(query)

    output: list[dict[str, Any]] = []
    details: list[dict[str, Any]] = []
    counts: Counter[str] = Counter()
    seen_expected: set[tuple[str, int]] = set()

    for line_no, original in enumerate(inference, 1):
        if original.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"{a.v11_inference_jsonl}:{line_no}: expected {INFERENCE_FORMAT}")
        row = dict(original)
        key = key_of(row, f"{a.v11_inference_jsonl}:{line_no}")
        if key not in EXPECTED:
            output.append(row)
            continue

        seen_expected.add(key)
        counts["eligible"] += 1
        expected = EXPECTED[key]
        audit = audits[key]
        reasons: list[str] = []

        if row.get("status") != "receiver_not_single_pointer":
            reasons.append("v11_row_not_receiver_not_single_pointer")
        if normalize_slot(row.get("dispatch_slot_offset")) != expected["slot"]:
            reasons.append("v11_slot_mismatch")
        if key in query_by_key:
            reasons.append("query_already_exists")

        if audit.get("status") != "singleton_cell_constructor_store_return_unique_class":
            reasons.append("singleton_audit_status_not_accepted")
        if audit.get("reasons") != []:
            reasons.append("singleton_audit_has_reasons")
        if audit.get("affects_routing") is not False:
            reasons.append("singleton_audit_routing_contract_invalid")
        if normalize_address(audit.get("singleton_cell")) != expected["cell"]:
            reasons.append("singleton_cell_mismatch")
        if audit.get("accessor_function_name") != expected["accessor"]:
            reasons.append("accessor_identity_mismatch")
        if normalize_slot(audit.get("dispatch_slot_offset")) != expected["slot"]:
            reasons.append("singleton_audit_slot_mismatch")
        if not isinstance(audit.get("exact_store_count"), int) or audit.get("exact_store_count", 0) < 1:
            reasons.append("exact_cell_store_missing")
        if not isinstance(audit.get("exact_load_count"), int) or audit.get("exact_load_count", 0) < 1:
            reasons.append("exact_cell_load_missing")
        returned_roots = audit.get("returned_exact_load_roots")
        if not isinstance(returned_roots, list) or not returned_roots or not all(isinstance(x, str) and x for x in returned_roots):
            reasons.append("exact_cell_load_not_returned")
        ctor_matches = audit.get("constructor_store_matches")
        if not isinstance(ctor_matches, list) or len(ctor_matches) < 1:
            reasons.append("constructor_store_match_missing")

        classes = unique_string_list(audit.get("candidate_classes"))
        vtables = unique_string_list(audit.get("candidate_vtables"))
        targets = unique_string_list(audit.get("slot_target_ids"))
        if classes is None:
            reasons.append("singleton_class_not_unique")
        if vtables is None:
            reasons.append("singleton_vtable_not_unique")
        if targets is None:
            reasons.append("singleton_slot_target_not_unique")

        receiver_class = classes[0] if classes else None
        vtable = vtables[0] if vtables else None
        target = targets[0] if targets else None
        catalog_row: dict[str, Any] | None = None
        if vtable is not None:
            rows = catalog_index.get((vtable, expected["slot"]), [])
            if len(rows) != 1:
                reasons.append("catalog_exact_vtable_slot_not_unique")
            else:
                catalog_row = rows[0]
                if catalog_row.get("class_name") != receiver_class:
                    reasons.append("catalog_class_mismatch")
                if catalog_row.get("static_status") != "single_function_memory_pointer":
                    reasons.append("catalog_slot_not_single_function")
                catalog_targets = catalog_row.get("candidate_target_ids")
                if not isinstance(catalog_targets, list) or catalog_targets != [target]:
                    reasons.append("catalog_target_mismatch")
                if catalog_row.get("runtime_validation_status") == "runtime_mismatch":
                    reasons.append("catalog_runtime_mismatch")

        recovery: dict[str, Any] = {
            "format": RECOVERY_FORMAT,
            "source_id": key[0],
            "call_index": key[1],
            "accepted": False,
            "singleton_cell": expected["cell"],
            "accessor_function_name": expected["accessor"],
            "dispatch_slot_offset": expected["slot"],
            "candidate_receiver_class_names": classes or [],
            "candidate_receiver_vtable_ids": vtables or [],
            "candidate_target_ids": targets or [],
            "catalog_static_status": catalog_row.get("static_status") if catalog_row else None,
            "catalog_runtime_validation_status": catalog_row.get("runtime_validation_status") if catalog_row else None,
            "reasons": reasons,
            "affects_routing": False,
        }

        if not reasons and receiver_class is not None and vtable is not None and target is not None:
            recovery["accepted"] = True
            counts["accepted"] += 1
            row["class_name"] = receiver_class
            row["class_match_kind"] = "exact_singleton_cell_constructor_store_return_catalog_class"
            row["class_evidence_source"] = "exact_singleton_cell_constructor_store_return_provenance"
            row["candidate_receiver_class_names"] = [receiver_class]
            row["candidate_receiver_vtable_ids"] = [vtable]
            row["receiver_vtable_id"] = vtable
            row["status"] = "resolved_unique_vtable"
            row["exact_singleton_cell_recovery"] = recovery

            query_by_key[key] = {
                "format": QUERY_FORMAT,
                "source_id": key[0],
                "call_index": key[1],
                "call_address": row.get("call_address"),
                "receiver_vtable_id": vtable,
                "dispatch_slot_offset": expected["slot"],
                "receiver_class_name": receiver_class,
                "receiver_class_match_kind": row["class_match_kind"],
                "receiver_class_evidence_source": row["class_evidence_source"],
                "receiver_type_name": row.get("receiver_type_name"),
                "receiver_type_display": row.get("receiver_type_display"),
                "runtime_validation_status": catalog_row.get("runtime_validation_status", "runtime_unobserved") if catalog_row else "runtime_unobserved",
                "evidence_ref": f"{key[0]}#call={key[1]}",
                "affects_routing": False,
            }
        else:
            counts["blocked"] += 1
            row["exact_singleton_cell_recovery"] = recovery

        details.append(recovery)
        output.append(row)

    if seen_expected != set(EXPECTED):
        raise ValueError(f"expected v11 singleton rows changed: missing={sorted(set(EXPECTED)-seen_expected)}")

    output_queries = [query_by_key[key] for key in sorted(query_by_key)]
    resolved_keys = {key_of(row, "output") for row in output if row.get("status") in RESOLVED}
    query_keys = set(query_by_key)
    if resolved_keys != query_keys:
        raise ValueError(
            "v12 resolved/query key mismatch: "
            f"missing_queries={sorted(resolved_keys-query_keys)} extra_queries={sorted(query_keys-resolved_keys)}"
        )

    write_jsonl(a.output, output)
    write_jsonl(a.queries_output, output_queries)
    if a.details_output is not None:
        write_jsonl(a.details_output, details)

    print("format:", FINAL_FORMAT)
    print("inference rows:", len(inference))
    print("input queries:", len(queries))
    print("eligible exact-singleton rows:", counts["eligible"])
    print("exact-singleton recoveries accepted:", counts["accepted"])
    print("exact-singleton recoveries blocked:", counts["blocked"])
    print("output queries:", len(output_queries))
    print("affects routing: no")
    print("output:", a.output)
    print("queries output:", a.queries_output)
    if a.details_output is not None:
        print("details output:", a.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
