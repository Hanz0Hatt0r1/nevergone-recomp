#!/usr/bin/env python3
"""Resolve only the three quarantined ControlsButton field receivers after packaged closure.

The old caller-argument convergence remains explicitly rejected. v14 accepts
only exact nested receiver-field provenance plus a fresh
ControlsButtonPackagedFieldStability/1 proof for the pinned APK, the complete
14-site external full-Controls-this frontier closure, and an exact
ControlsButton vtable/slot row in StaticVtableSlotCatalog/3.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
QUERY_FORMAT = "NEVERGONE.StaticReceiverVtableQuery/1"
STABILITY_FORMAT = "NEVERGONE.ControlsButtonPackagedFieldStability/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
DETAIL_FORMAT = "NEVERGONE.ControlsButtonFieldRecovery/1"
FINAL_FORMAT = "NEVERGONE.ControlsButtonFieldRecoveryFinalization/1"
SOURCE = "libcocos2dcpp.so:002a7594"
SLOT = "0x80"
CLASS = "ControlsButton"
VTABLE = "libcocos2dcpp.so:008f46f8"
TARGET = "libcocos2dcpp.so:00525d2e"
CALLS = {
    3: ("002a7612", "0x134"),
    4: ("002a7634", "0x130"),
    7: ("002a7686", "0x12c"),
}
RESOLVED = {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match", "resolved_converged_target"}


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows = []
    with path.open(encoding="utf-8") as handle:
        for line_number, line in enumerate(handle, 1):
            if not line.strip():
                continue
            row = json.loads(line)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_number}: expected JSON object")
            rows.append(row)
    return rows


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def key(row: dict[str, Any], where: str) -> tuple[str, int]:
    source = row.get("source_id")
    call = row.get("call_index")
    if not isinstance(source, str) or not isinstance(call, int):
        raise ValueError(f"{where}: invalid source/call identity")
    return source, call


def norm_slot(value: Any) -> str | None:
    try:
        return f"0x{int(str(value), 0):x}"
    except Exception:
        return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("v13_inference_jsonl", type=Path)
    parser.add_argument("v13_queries_jsonl", type=Path)
    parser.add_argument("packaged_field_stability_jsonl", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--queries-output", required=True, type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    inference = read_jsonl(args.v13_inference_jsonl)
    queries = read_jsonl(args.v13_queries_jsonl)
    stability = read_jsonl(args.packaged_field_stability_jsonl)
    catalog = read_jsonl(args.catalog_jsonl)

    summaries = [row for row in stability if row.get("format") == STABILITY_FORMAT and
                 row.get("record_type") == "summary"]
    if len(summaries) != 1:
        raise ValueError("expected exactly one packaged field stability summary")
    summary = summaries[0]
    if not (summary.get("scope") == "exact_packaged_apk_normal_code_execution" and
            summary.get("target_fields") == ["0x12c", "0x130", "0x134"] and
            summary.get("target_calls") == [3, 4, 7] and
            summary.get("packaged_controlsbutton_field_stability_proven") is True and
            summary.get("packaged_receiver_class_proven") is True and
            summary.get("caller_argument_quarantine_preserved") is True and
            summary.get("external_full_this_static_frontier_closed") is True and
            summary.get("external_full_this_sites_closed") == 14 and
            summary.get("known_external_frontier_writes_receiver_fields") is False and
            summary.get("whole_program_alias_writes_excluded") is False and
            summary.get("external_injected_code_excluded") is False and
            summary.get("affects_routing") is False):
        raise ValueError("packaged field stability proof is not acceptable")

    stability_by_call = {}
    for row in stability:
        if row.get("format") != STABILITY_FORMAT or row.get("record_type") != "receiver_field":
            continue
        k = key(row, "stability")
        if k in stability_by_call:
            raise ValueError(f"duplicate stability receiver row: {k}")
        stability_by_call[k] = row
    expected_keys = {(SOURCE, call) for call in CALLS}
    if set(stability_by_call) != expected_keys:
        raise ValueError("stability rows do not match three quarantined calls")

    query_by_key: dict[tuple[str, int], dict[str, Any]] = {}
    for number, query in enumerate(queries, 1):
        if query.get("format") != QUERY_FORMAT:
            raise ValueError(f"query:{number}: expected {QUERY_FORMAT}")
        k = key(query, f"query:{number}")
        if k in query_by_key:
            raise ValueError(f"duplicate query: {k}")
        query_by_key[k] = dict(query)
    if expected_keys & set(query_by_key):
        raise ValueError("v13 already contains query for quarantined call")

    catalog_rows = [row for row in catalog if
                    row.get("format") == CATALOG_FORMAT and
                    row.get("class_name") == CLASS and
                    row.get("receiver_vtable_id") == VTABLE and
                    norm_slot(row.get("dispatch_slot_offset")) == SLOT]
    if len(catalog_rows) != 1:
        raise ValueError("exact ControlsButton vtable slot catalog row is not unique")
    catalog_row = catalog_rows[0]
    if not (catalog_row.get("static_status") == "single_function_memory_pointer" and
            catalog_row.get("candidate_target_ids") == [TARGET] and
            catalog_row.get("runtime_validation_status") != "runtime_mismatch"):
        raise ValueError("ControlsButton slot 0x80 catalog evidence changed")

    output = []
    details = []
    accepted = 0
    seen = set()
    for number, original in enumerate(inference, 1):
        if original.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"inference:{number}: expected {INFERENCE_FORMAT}")
        row = dict(original)
        k = key(row, f"inference:{number}")
        if k not in expected_keys:
            output.append(row)
            continue
        seen.add(k)
        call = k[1]
        expected_address, expected_field = CALLS[call]
        proof = stability_by_call[k]
        reasons = []
        if row.get("status") != "receiver_not_single_pointer":
            reasons.append("v13_row_not_receiver_not_single_pointer")
        if row.get("call_address") != expected_address:
            reasons.append("v13_call_address_mismatch")
        if norm_slot(row.get("dispatch_slot_offset")) != SLOT:
            reasons.append("v13_slot_mismatch")
        mixed = row.get("mixed_slot_target_recovery")
        if not isinstance(mixed, dict) or mixed.get("accepted") is not False or \
                mixed.get("reasons") != ["caller_argument_not_virtual_receiver_provenance"]:
            reasons.append("old_caller_argument_quarantine_not_preserved")
        if proof.get("call_address") != expected_address:
            reasons.append("stability_call_address_mismatch")
        if proof.get("receiver_field_offset") != expected_field:
            reasons.append("stability_field_offset_mismatch")
        if proof.get("dispatch_slot_offset") != SLOT:
            reasons.append("stability_slot_mismatch")
        if proof.get("receiver_class_name") != CLASS:
            reasons.append("stability_class_mismatch")
        if proof.get("receiver_vtable_id") != VTABLE:
            reasons.append("stability_vtable_mismatch")
        if proof.get("slot_target_id") != TARGET:
            reasons.append("stability_target_mismatch")
        if proof.get("actual_nested_receiver_provenance") is not True:
            reasons.append("actual_nested_receiver_not_proven")
        if proof.get("packaged_field_stability_proven") is not True or \
                proof.get("packaged_receiver_class_proven") is not True:
            reasons.append("packaged_stability_not_proven")
        if proof.get("external_full_this_static_frontier_closed") is not True:
            reasons.append("external_full_this_frontier_not_closed")
        if proof.get("caller_argument_provenance_used") is not False:
            reasons.append("caller_argument_provenance_reintroduced")
        if proof.get("affects_routing") is not False:
            reasons.append("stability_routing_contract_invalid")

        recovery = {
            "format": DETAIL_FORMAT, "source_id": SOURCE, "call_index": call,
            "call_address": expected_address, "dispatch_slot_offset": SLOT,
            "receiver_field_offset": expected_field, "accepted": False,
            "receiver_class_name": CLASS, "receiver_vtable_id": VTABLE,
            "candidate_target_ids": [TARGET], "packaged_scope_only": True,
            "external_full_this_static_frontier_closed": True,
            "caller_argument_provenance_used": False,
            "whole_program_alias_writes_excluded": False,
            "reasons": reasons, "affects_routing": False,
        }
        if not reasons:
            recovery["accepted"] = True
            accepted += 1
            row["class_name"] = CLASS
            row["class_match_kind"] = "controlsbutton_exact_field_packaged_stability"
            row["class_evidence_source"] = "actual_receiver_field_factory_packaged_alias_closure"
            row["candidate_receiver_class_names"] = [CLASS]
            row["candidate_receiver_vtable_ids"] = [VTABLE]
            row["receiver_vtable_id"] = VTABLE
            row["status"] = "resolved_unique_vtable"
            row["controlsbutton_field_recovery_v14"] = recovery
            query_by_key[k] = {
                "format": QUERY_FORMAT, "source_id": SOURCE, "call_index": call,
                "call_address": expected_address, "receiver_vtable_id": VTABLE,
                "dispatch_slot_offset": SLOT, "receiver_class_name": CLASS,
                "receiver_class_match_kind": row["class_match_kind"],
                "receiver_class_evidence_source": row["class_evidence_source"],
                "receiver_type_name": row.get("receiver_type_name"),
                "receiver_type_display": row.get("receiver_type_display"),
                "runtime_validation_status": catalog_row.get("runtime_validation_status", "runtime_unobserved"),
                "evidence_ref": f"{SOURCE}#call={call}", "affects_routing": False,
            }
        else:
            row["controlsbutton_field_recovery_v14"] = recovery
        details.append(recovery)
        output.append(row)

    if seen != expected_keys:
        raise ValueError(f"v13 missing quarantined rows: {sorted(expected_keys - seen)}")
    if accepted != 3:
        raise ValueError(f"expected all three v14 recoveries accepted, got {accepted}")

    out_queries = [query_by_key[k] for k in sorted(query_by_key)]
    resolved_keys = {key(row, "output") for row in output if row.get("status") in RESOLVED}
    if resolved_keys != set(query_by_key):
        raise ValueError("v14 resolved/query identity mismatch")

    write_jsonl(args.output, output)
    write_jsonl(args.queries_output, out_queries)
    if args.details_output:
        write_jsonl(args.details_output, details)
    print("format:", FINAL_FORMAT)
    print("inference rows:", len(inference))
    print("input queries:", len(queries))
    print("ControlsButton packaged field recoveries accepted:", accepted)
    print("external full-this frontier: 14/14 closed")
    print("output queries:", len(out_queries))
    print("caller-argument quarantine reused: no")
    print("affects routing: no")
    print("output:", args.output)
    print("queries output:", args.queries_output)
    if args.details_output:
        print("details output:", args.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
