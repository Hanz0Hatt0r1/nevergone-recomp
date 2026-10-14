#!/usr/bin/env python3
"""Quarantine legacy mixed caller-argument recovery until receiver values are proven.

The original convergence checks traced the caller's parameter, not the object
used by nested CALLIND dispatch. This finalizer preserves the evidence for
diagnostics, but blocks all three legacy recoveries (including stale "successful"
strict audit JSONL). No query is emitted for these calls.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
QUERY_FORMAT = "NEVERGONE.StaticReceiverVtableQuery/1"
AUDIT_FORMAT = "NEVERGONE.GameSceneUIControlsSlotConvergenceAudit/1"
STRICT_AUDIT_FORMAT = "NEVERGONE.MixedReceiverSlotTargetConvergenceAudit/1"
TARGET_SOURCE = "libcocos2dcpp.so:002a7594"
TARGET_CALLS = {3, 4, 7}
RECOVERY_FORMAT = "NEVERGONE.MixedReceiverSlotTargetRecovery/1"
RESOLVED = {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match", "resolved_converged_target"}
MODE = "audited_multi_vtable_target_convergence"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            value = json.loads(raw)
            if not isinstance(value, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            rows.append(value)
    return rows


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def key_of(row: dict[str, Any], where: str) -> tuple[str, int]:
    source_id = row.get("source_id")
    call_index = row.get("call_index")
    if not isinstance(source_id, str) or not source_id or not isinstance(call_index, int) or call_index < 0:
        raise ValueError(f"{where}: invalid source_id/call_index")
    return source_id, call_index


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


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("v9_inference_jsonl", type=Path)
    p.add_argument("v9_queries_jsonl", type=Path)
    p.add_argument("slot_convergence_audit_jsonl", type=Path)
    p.add_argument("--strict-mixed-audit-jsonl", type=Path, required=True,
                   help="Per-call audit proving both receiver branches, including singleton provenance")
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--queries-output", type=Path, required=True)
    p.add_argument("--details-output", type=Path)
    a = p.parse_args()

    inference = read_jsonl(a.v9_inference_jsonl)
    queries = read_jsonl(a.v9_queries_jsonl)
    audit_rows = read_jsonl(a.slot_convergence_audit_jsonl)
    strict_rows = read_jsonl(a.strict_mixed_audit_jsonl)
    if len(audit_rows) != 1 or audit_rows[0].get("format") != AUDIT_FORMAT:
        raise ValueError(f"{a.slot_convergence_audit_jsonl}: expected exactly one {AUDIT_FORMAT} row")
    audit = audit_rows[0]

    source_id = audit.get("source_id")
    call_indices = audit.get("call_indices")
    slot = normalize_slot(audit.get("dispatch_slot_offset"))
    branch_classes = audit.get("branch_classes")
    class_slots = audit.get("class_slots")
    target = audit.get("converged_target_id")

    # Fail closed even on stale, previously "successful" strict audit JSONL.
    # The old branch proof followed the CALLER ARGUMENT to
    # Controls::showControlsAttackKey, not the nested CALLIND receiver.
    # These exact calls require a new receiver-value-specific recovery path.
    contract_reasons: list[str] = ["caller_argument_not_virtual_receiver_provenance"]
    if audit.get("status") != "mixed_receiver_slot_target_converges":
        contract_reasons.append("audit_status_not_converged")
    if audit.get("controls_field_class_proven") is not True:
        contract_reasons.append("controls_field_class_not_proven")
    if audit.get("target_rows_stable") is not True:
        contract_reasons.append("target_rows_not_stable")
    if source_id != TARGET_SOURCE:
        contract_reasons.append("audit_source_invalid")
    if not isinstance(call_indices, list) or len(call_indices) != 3 or sorted(call_indices) != [3, 4, 7]:
        contract_reasons.append("audit_call_set_invalid")
    if slot != "0x80":
        contract_reasons.append("audit_slot_invalid")
    if not isinstance(branch_classes, list) or set(branch_classes) != {"GameSceneUI", "Controls"}:
        contract_reasons.append("audit_branch_classes_invalid")
    if not isinstance(target, str) or not target:
        contract_reasons.append("audit_target_invalid")
    if not isinstance(class_slots, list) or len(class_slots) != 2:
        contract_reasons.append("audit_class_slots_invalid")

    candidate_vtables: list[str] = []
    if isinstance(class_slots, list):
        seen_classes: set[str] = set()
        for item in class_slots:
            if not isinstance(item, dict):
                contract_reasons.append("audit_class_slot_not_object")
                continue
            cls = item.get("class_name")
            vt = item.get("receiver_vtable_id")
            ids = item.get("candidate_target_ids")
            if not isinstance(cls, str) or cls not in {"GameSceneUI", "Controls"}:
                contract_reasons.append("audit_class_slot_class_invalid")
            else:
                seen_classes.add(cls)
            if not isinstance(vt, str) or not vt:
                contract_reasons.append("audit_class_slot_vtable_invalid")
            else:
                candidate_vtables.append(vt)
            if item.get("dispatch_slot_offset") != "0x80":
                contract_reasons.append("audit_class_slot_offset_invalid")
            if item.get("static_status") != "single_function_memory_pointer":
                contract_reasons.append("audit_class_slot_not_single_function")
            if not isinstance(ids, list) or ids != [target]:
                contract_reasons.append("audit_class_slot_target_mismatch")
        if seen_classes != {"GameSceneUI", "Controls"}:
            contract_reasons.append("audit_class_slot_class_set_invalid")
    candidate_vtables = sorted(set(candidate_vtables))
    if len(candidate_vtables) != 2:
        contract_reasons.append("audit_candidate_vtable_set_invalid")

    # The older slot-only audit proves equal catalog targets but does not
    # prove the GameSceneUI singleton branch. Require the independent strict
    # per-call mixed-receiver audit before allowing any v11 recovery.
    strict_by_call: dict[int, dict[str, Any]] = {}
    if len(strict_rows) != 3:
        contract_reasons.append("strict_mixed_receiver_row_count_invalid")
    for item in strict_rows:
        if item.get("format") != STRICT_AUDIT_FORMAT:
            contract_reasons.append("strict_mixed_receiver_format_invalid")
            continue
        call = item.get("call_index")
        if item.get("source_id") != TARGET_SOURCE or not isinstance(call, int) or call not in TARGET_CALLS:
            contract_reasons.append("strict_mixed_receiver_key_invalid")
            continue
        if call in strict_by_call:
            contract_reasons.append("strict_mixed_receiver_duplicate_call")
            continue
        strict_by_call[call] = item
        if (
            item.get("status") != "unique_target_across_proven_receiver_classes"
            or item.get("reasons") != []
            or item.get("affects_routing") is not False
        ):
            contract_reasons.append(f"strict_mixed_receiver_call_{call}_not_proven")
        if normalize_slot(item.get("dispatch_slot_offset")) != slot:
            contract_reasons.append(f"strict_mixed_receiver_call_{call}_slot_mismatch")
        classes = item.get("proven_receiver_class_names")
        if not isinstance(classes, list) or not all(isinstance(cls, str) for cls in classes) or sorted(classes) != ["Controls", "GameSceneUI"]:
            contract_reasons.append(f"strict_mixed_receiver_call_{call}_classes_mismatch")
        vtables = item.get("proven_receiver_vtable_ids")
        if not isinstance(vtables, list) or len(vtables) != 2 or not all(isinstance(vt, str) for vt in vtables) or sorted(vtables) != candidate_vtables:
            contract_reasons.append(f"strict_mixed_receiver_call_{call}_vtables_mismatch")
        if item.get("class_target_ids") != {"Controls": [target], "GameSceneUI": [target]}:
            contract_reasons.append(f"strict_mixed_receiver_call_{call}_targets_mismatch")
        if item.get("convergent_target_id") != target or item.get("controls_field_offset") != "0x134":
            contract_reasons.append(f"strict_mixed_receiver_call_{call}_field_or_target_mismatch")
    if set(strict_by_call) != TARGET_CALLS:
        contract_reasons.append("strict_mixed_receiver_call_set_incomplete")

    query_by_key: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, q in enumerate(queries, 1):
        if q.get("format") != QUERY_FORMAT:
            raise ValueError(f"{a.v9_queries_jsonl}:{line_no}: expected {QUERY_FORMAT}")
        key = key_of(q, f"{a.v9_queries_jsonl}:{line_no}")
        if key in query_by_key:
            raise ValueError(f"duplicate query: {key}")
        query_by_key[key] = dict(q)

    outputs: list[dict[str, Any]] = []
    details: list[dict[str, Any]] = []
    counts: Counter[str] = Counter()
    expected_keys = {(source_id, i) for i in call_indices} if isinstance(source_id, str) and isinstance(call_indices, list) else set()
    seen_eligible: set[tuple[str, int]] = set()

    for line_no, original in enumerate(inference, 1):
        if original.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"{a.v9_inference_jsonl}:{line_no}: expected {INFERENCE_FORMAT}")
        row = dict(original)
        key = key_of(row, f"{a.v9_inference_jsonl}:{line_no}")
        if key not in expected_keys:
            outputs.append(row)
            continue

        seen_eligible.add(key)
        counts["eligible"] += 1
        reasons = list(contract_reasons)
        if row.get("status") != "receiver_not_single_pointer":
            reasons.append("inference_row_not_receiver_not_single_pointer")
        if normalize_slot(row.get("dispatch_slot_offset")) != slot:
            reasons.append("inference_slot_mismatch")
        if key in query_by_key:
            reasons.append("query_already_exists")

        recovery = {
            "format": RECOVERY_FORMAT,
            "source_id": key[0],
            "call_index": key[1],
            "accepted": False,
            # Historical branches are caller-argument candidates, not
            # independently proven CALLIND receiver candidates.
            "branch_classes": [],
            "candidate_receiver_vtable_ids": [],
            "caller_argument_branch_classes": branch_classes,
            "caller_argument_candidate_vtable_ids": candidate_vtables,
            "dispatch_slot_offset": slot,
            "converged_target_id": target,
            "resolution_mode": MODE,
            "reasons": reasons,
            "affects_routing": False,
        }

        if not reasons:
            recovery["accepted"] = True
            counts["accepted"] += 1
            row["class_name"] = None
            row["class_match_kind"] = "mixed_receiver_classes_exact_slot_target_convergence"
            row["class_evidence_source"] = "audited_gamesceneui_controls_slot_convergence"
            row["candidate_receiver_class_names"] = sorted(branch_classes)
            row["candidate_receiver_vtable_ids"] = candidate_vtables
            row["receiver_vtable_id"] = None
            row["converged_target_id"] = target
            row["resolution_mode"] = MODE
            row["status"] = "resolved_converged_target"
            row["mixed_slot_target_recovery"] = recovery
            query_by_key[key] = {
                "format": QUERY_FORMAT,
                "source_id": key[0],
                "call_index": key[1],
                "call_address": row.get("call_address"),
                "receiver_vtable_id": None,
                "candidate_receiver_vtable_ids": candidate_vtables,
                "dispatch_slot_offset": slot,
                "receiver_class_name": None,
                "candidate_receiver_class_names": sorted(branch_classes),
                "receiver_class_match_kind": row["class_match_kind"],
                "receiver_class_evidence_source": row["class_evidence_source"],
                "resolution_mode": MODE,
                "converged_target_id": target,
                "runtime_validation_status": "runtime_unobserved",
                "evidence_ref": f"{key[0]}#call={key[1]}",
                "affects_routing": False,
            }
        else:
            counts["blocked"] += 1
            row["mixed_slot_target_recovery"] = recovery
        details.append(recovery)
        outputs.append(row)

    if seen_eligible != expected_keys:
        raise ValueError(f"expected mixed recovery rows changed: missing={sorted(expected_keys-seen_eligible)} extra={sorted(seen_eligible-expected_keys)}")

    output_queries = [query_by_key[k] for k in sorted(query_by_key)]
    resolved_keys = {key_of(r, "output") for r in outputs if r.get("status") in RESOLVED}
    if resolved_keys != set(query_by_key):
        raise ValueError(f"resolved/query key mismatch: missing={sorted(resolved_keys-set(query_by_key))} extra={sorted(set(query_by_key)-resolved_keys)}")

    write_jsonl(a.output, outputs)
    write_jsonl(a.queries_output, output_queries)
    if a.details_output is not None:
        write_jsonl(a.details_output, details)

    print("format: NEVERGONE.MixedReceiverSlotTargetRecoveryFinalization/1")
    print("inference rows:", len(inference))
    print("input queries:", len(queries))
    print("eligible mixed-slot rows:", counts["eligible"])
    print("mixed-slot target recoveries accepted:", counts["accepted"])
    print("mixed-slot target recoveries blocked:", counts["blocked"])
    print("output queries:", len(output_queries))
    print("resolution mode:", MODE)
    print("affects routing: no")
    print("output:", a.output)
    print("queries output:", a.queries_output)
    if a.details_output is not None:
        print("details output:", a.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
