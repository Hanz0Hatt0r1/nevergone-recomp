#!/usr/bin/env python3
"""Resolve the final three ControlsButton sprite slot-0x80 receivers after v16.

This stage joins two independent evidence surfaces:

* exact structural receiver-field/factory candidates for 002a87d2 calls 3/4/5;
* packaged owner-field stability for ControlsButton+0x104/+0x108/+0x10c.

The packaged stability artifact deliberately does not prove a receiver class on
its own. The finalizer only promotes a row after the per-call receiver slice,
factory witnesses, field identity, class/vtable/slot catalog and packaged
owner-field closure all agree.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path
from typing import Any

INF = "NEVERGONE.StaticReceiverVtableInference/1"
QUERY = "NEVERGONE.StaticReceiverVtableQuery/1"
STABILITY = "NEVERGONE.ControlsButtonOwnerFieldPackagedStability/1"
CANDIDATE = "NEVERGONE.Slot80FactoryFieldCandidate/1"
CATALOG = "NEVERGONE.StaticVtableSlotCatalog/3"
DETAIL = "NEVERGONE.ControlsButtonSpriteFieldRecovery/1"
FINAL = "NEVERGONE.ControlsButtonSpriteFieldRecoveryFinalization/1"
SLOT = "0x80"
CLASS = "cocos2d::CCSprite"
VTABLE = "libcocos2dcpp.so:009363d8"
TARGET = "libcocos2dcpp.so:00549f1e"
CALLS = {
    ("libcocos2dcpp.so:002a87d2", 3): "0x104",
    ("libcocos2dcpp.so:002a87d2", 4): "0x10c",
    ("libcocos2dcpp.so:002a87d2", 5): "0x108",
}
FIELD_STORES = {
    "0x104": {"0x2984cc", "0x2985c4"},
    "0x108": {"0x2984ea", "0x2985de"},
    "0x10c": {"0x298510"},
}
RESOLVED = {
    "resolved_unique_vtable",
    "resolved_unique_vtable_runtime_match",
    "resolved_converged_target",
}


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
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
    path.write_text(
        "".join(json.dumps(row, sort_keys=True) + "\n" for row in rows),
        encoding="utf-8",
    )


def row_key(row: dict[str, Any], where: str) -> tuple[str, int]:
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


def verify_stability(rows: list[dict[str, Any]]) -> dict[str, Any]:
    if len(rows) != 1:
        raise ValueError("expected exactly one ControlsButton owner stability row")
    row = rows[0]
    if not (
        row.get("format") == STABILITY
        and row.get("scope") == "exact_packaged_apk_normal_code_execution"
        and row.get("target_fields") == ["0x104", "0x108", "0x10c"]
        and row.get("first_hop_closed") is True
        and row.get("owner_fields_stable") is True
        and row.get("receiver_class_proven") is False
        and row.get("packaged_script_engine_write_through_reachable") is False
        and row.get("packaged_dlsym_setter_lookup_excluded") is True
        and row.get("whole_program_alias_writes_excluded") is False
        and row.get("external_injected_code_excluded") is False
        and row.get("affects_routing") is False
    ):
        raise ValueError("ControlsButton owner packaged stability rejected")

    ccpoint = row.get("ccpoint_subobject_channel") or {}
    touch = row.get("targeted_touch_channel") or {}
    parent = row.get("parent_backref_channel") or {}
    lifecycle = row.get("cclayer_lifecycle_channel") or {}
    if not (
        ccpoint.get("subobject_offset") == "0x120"
        and ccpoint.get("call_count") == 7
        and ccpoint.get("can_recover_owner") is False
        and touch.get("retained_delegate") == "ControlsButton+0xe0"
        and touch.get("callback_methods_in_19_method_inventory") is True
        and touch.get("target_fields_written_by_touch_callbacks_or_first_hop") is False
        and parent.get("parent_targets_write_sprite_fields") is False
        and lifecycle.get("lifecycle_directly_writes_sprite_fields") is False
    ):
        raise ValueError("ControlsButton owner packaged closure channels changed")
    return row


def verify_candidates(rows: list[dict[str, Any]]) -> dict[tuple[str, int], dict[str, Any]]:
    selected: dict[tuple[str, int], dict[str, Any]] = {}
    for number, row in enumerate(rows, 1):
        if row.get("format") != CANDIDATE:
            raise ValueError(f"candidate:{number}: wrong format")
        key = row_key(row, f"candidate:{number}")
        if key not in CALLS:
            continue
        if key in selected:
            raise ValueError(f"duplicate selected candidate: {key}")
        field = CALLS[key]
        address = row.get("call_address")
        witnesses = row.get("factory_witnesses")
        stores = {
            witness.get("store_elf")
            for witness in witnesses or []
            if isinstance(witness, dict)
        }
        if not (
            isinstance(address, str)
            and re.fullmatch(r"[0-9a-f]{8}", address) is not None
            and norm_slot(row.get("dispatch_slot_offset")) == SLOT
            and row.get("owner_class") == "ControlsButton"
            and row.get("owner_field_offset") == field
            and row.get("factory_class_candidate") == CLASS
            and row.get("candidate_receiver_vtable_id") == VTABLE
            and row.get("candidate_target_id") == TARGET
            and row.get("status") == "construction_linked_target_candidate"
            and row.get("field_writes_exhaustive") is False
            and row.get("receiver_class_proven") is False
            and row.get("affects_routing") is False
            and stores == FIELD_STORES[field]
        ):
            raise ValueError(f"selected ControlsButton sprite candidate changed: {key}")
        selected[key] = row
    if set(selected) != set(CALLS):
        raise ValueError(
            "expected exactly three ControlsButton sprite candidates; "
            f"missing={sorted(set(CALLS) - set(selected))}"
        )
    return selected


def verify_catalog(rows: list[dict[str, Any]]) -> dict[str, Any]:
    matched = [
        row
        for row in rows
        if row.get("format") == CATALOG
        and row.get("class_name") == CLASS
        and row.get("receiver_vtable_id") == VTABLE
        and norm_slot(row.get("dispatch_slot_offset")) == SLOT
    ]
    if len(matched) != 1:
        raise ValueError("exact CCSprite slot-0x80 catalog row is not unique")
    row = matched[0]
    if not (
        row.get("static_status") == "single_function_memory_pointer"
        and row.get("candidate_target_ids") == [TARGET]
        and row.get("runtime_validation_status") != "runtime_mismatch"
        and row.get("cross_class_transfer_allowed") is False
    ):
        raise ValueError("CCSprite slot-0x80 catalog evidence changed")
    return row


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("v16_inference_jsonl", type=Path)
    parser.add_argument("v16_queries_jsonl", type=Path)
    parser.add_argument("owner_packaged_stability_jsonl", type=Path)
    parser.add_argument("slot80_factory_candidates_jsonl", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--queries-output", required=True, type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    inference = read_jsonl(args.v16_inference_jsonl)
    queries = read_jsonl(args.v16_queries_jsonl)
    verify_stability(read_jsonl(args.owner_packaged_stability_jsonl))
    candidates = verify_candidates(read_jsonl(args.slot80_factory_candidates_jsonl))
    catalog = verify_catalog(read_jsonl(args.catalog_jsonl))

    query_by_key: dict[tuple[str, int], dict[str, Any]] = {}
    for number, query in enumerate(queries, 1):
        if query.get("format") != QUERY:
            raise ValueError(f"query:{number}: wrong format")
        key = row_key(query, f"query:{number}")
        if key in query_by_key:
            raise ValueError(f"duplicate query: {key}")
        query_by_key[key] = dict(query)
    if set(CALLS) & set(query_by_key):
        raise ValueError("v16 already contains a query for a v17 target")

    output: list[dict[str, Any]] = []
    details: list[dict[str, Any]] = []
    seen: set[tuple[str, int]] = set()
    accepted = 0

    for number, original in enumerate(inference, 1):
        if original.get("format") != INF:
            raise ValueError(f"inference:{number}: wrong format")
        row = dict(original)
        key = row_key(row, f"inference:{number}")
        if key not in CALLS:
            output.append(row)
            continue

        seen.add(key)
        field = CALLS[key]
        candidate = candidates[key]
        address = candidate["call_address"]
        reasons: list[str] = []
        if row.get("status") != "ambiguous_receiver_class":
            reasons.append("v16_row_not_ambiguous_receiver_class")
        if row.get("call_address") != address:
            reasons.append("candidate_call_address_mismatch")
        if norm_slot(row.get("dispatch_slot_offset")) != SLOT:
            reasons.append("slot_mismatch")
        if not (
            candidate.get("owner_field_offset") == field
            and candidate.get("factory_class_candidate") == CLASS
            and candidate.get("candidate_receiver_vtable_id") == VTABLE
            and candidate.get("candidate_target_id") == TARGET
        ):
            reasons.append("candidate_receiver_field_or_class_mismatch")

        recovery = {
            "format": DETAIL,
            "source_id": key[0],
            "call_index": key[1],
            "call_address": address,
            "dispatch_slot_offset": SLOT,
            "receiver_field_offset": field,
            "accepted": False,
            "receiver_class_name": CLASS,
            "receiver_vtable_id": VTABLE,
            "candidate_target_ids": [TARGET],
            "actual_receiver_field_slice_proven": True,
            "factory_receiver_class_candidate_proven": True,
            "packaged_owner_field_stability_proven": True,
            "first_hop_closed": True,
            "whole_program_alias_writes_excluded": False,
            "external_injected_code_excluded": False,
            "affects_routing": False,
            "reasons": reasons,
        }

        if not reasons:
            recovery["accepted"] = True
            accepted += 1
            row["class_name"] = CLASS
            row["class_match_kind"] = "controlsbutton_sprite_field_packaged_stability"
            row["class_evidence_source"] = (
                "slot80_receiver_field_factory_plus_controlsbutton_owner_packaged_closure"
            )
            row["candidate_receiver_class_names"] = [CLASS]
            row["candidate_receiver_vtable_ids"] = [VTABLE]
            row["receiver_vtable_id"] = VTABLE
            row["status"] = "resolved_unique_vtable"
            row["controlsbutton_sprite_field_recovery_v17"] = recovery
            query_by_key[key] = {
                "format": QUERY,
                "source_id": key[0],
                "call_index": key[1],
                "call_address": address,
                "receiver_vtable_id": VTABLE,
                "dispatch_slot_offset": SLOT,
                "receiver_class_name": CLASS,
                "receiver_class_match_kind": row["class_match_kind"],
                "receiver_class_evidence_source": row["class_evidence_source"],
                "receiver_type_name": row.get("receiver_type_name"),
                "receiver_type_display": row.get("receiver_type_display"),
                "runtime_validation_status": catalog.get(
                    "runtime_validation_status", "runtime_unobserved"
                ),
                "evidence_ref": f"{key[0]}#call={key[1]}",
                "affects_routing": False,
            }
        else:
            row["controlsbutton_sprite_field_recovery_v17"] = recovery

        details.append(recovery)
        output.append(row)

    if seen != set(CALLS):
        raise ValueError(f"v16 inference missing v17 rows: {sorted(set(CALLS) - seen)}")
    if accepted != 3:
        raise ValueError(f"expected three v17 recoveries, got {accepted}")

    output_queries = [query_by_key[key] for key in sorted(query_by_key)]
    resolved = {
        row_key(row, "output")
        for row in output
        if row.get("status") in RESOLVED
    }
    if resolved != set(query_by_key):
        raise ValueError("v17 resolved/query identity mismatch")

    write_jsonl(args.output, output)
    write_jsonl(args.queries_output, output_queries)
    if args.details_output:
        write_jsonl(args.details_output, details)

    print("format:", FINAL)
    print("ControlsButton CCSprite field recoveries accepted:", accepted)
    print("input queries:", len(queries), "output queries:", len(output_queries))
    print("ambiguous receiver rows promoted: 3")
    print("whole-program alias exclusion: no")
    print("output:", args.output)
    print("queries output:", args.queries_output)
    if args.details_output:
        print("details output:", args.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
