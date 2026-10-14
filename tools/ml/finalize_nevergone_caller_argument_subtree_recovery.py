#!/usr/bin/env python3
"""Resolve v9 receiver failures only from complete convergent caller arguments.

This fail-closed v10 post-finalizer consumes the evidence-only caller-argument
subtree probe. A row is recovered only when the persisted plan still matches the
v9 unresolved inference row, the subtree audit proves complete direct-caller
coverage, every matched caller argument converges to the same single catalog
class along value-preserving edges, and that class maps to exactly one catalog
vtable. Address/base LOAD evidence, incomplete coverage, mixed classes and
ambiguous class->vtable mappings remain unresolved.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
QUERY_FORMAT = "NEVERGONE.StaticReceiverVtableQuery/1"
AUDIT_FORMAT = "NEVERGONE.CallerArgumentSubtreeAudit/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
RECOVERY_FORMAT = "NEVERGONE.CallerArgumentSubtreeRecovery/1"
RESOLVED = {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match"}


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            rows.append(row)
    return rows


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def key_of(row: dict[str, Any], where: str) -> tuple[str, int]:
    source_id = row.get("source_id")
    call_index = row.get("call_index")
    if not isinstance(source_id, str) or not source_id or not isinstance(call_index, int) or call_index < 0:
        raise ValueError(f"{where}: invalid source_id/call_index")
    return source_id, call_index


def parse_decimal(value: Any, default: int = -1) -> int:
    if value in (None, "", "-"):
        return default
    text = str(value).strip()
    if not re.fullmatch(r"-?[0-9]+", text):
        raise ValueError(f"expected decimal integer, got {value!r}")
    return int(text, 10)


def normalize_hex(value: Any) -> str | None:
    if value in (None, "", "-"):
        return None
    text = str(value).strip()
    try:
        parsed = int(text, 0)
    except ValueError:
        if re.fullmatch(r"[0-9a-fA-F]+", text):
            parsed = int(text, 16)
        else:
            return None
    return f"0x{parsed:x}" if parsed >= 0 else None


def source_entry(source_id: str) -> str:
    return source_id.split(":", 1)[1] if ":" in source_id else source_id


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("v9_inference_jsonl", type=Path)
    parser.add_argument("v9_queries_jsonl", type=Path)
    parser.add_argument("caller_plan_tsv", type=Path)
    parser.add_argument("caller_audit_jsonl", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--queries-output", type=Path, required=True)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    inference = read_jsonl(args.v9_inference_jsonl)
    queries = read_jsonl(args.v9_queries_jsonl)

    plans: dict[tuple[str, int], dict[str, str]] = {}
    for line_no, row in enumerate(read_tsv(args.caller_plan_tsv), 2):
        source_id = row.get("source_id") or ""
        call_index = parse_decimal(row.get("source_call_index"), -1)
        if not source_id or call_index < 0:
            raise ValueError(f"{args.caller_plan_tsv}:{line_no}: invalid source/call")
        key = (source_id, call_index)
        if key in plans:
            raise ValueError(f"duplicate caller subtree plan row: {key}")
        plans[key] = row

    audits: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, row in enumerate(read_jsonl(args.caller_audit_jsonl), 1):
        if row.get("format") != AUDIT_FORMAT:
            raise ValueError(f"{args.caller_audit_jsonl}:{line_no}: expected {AUDIT_FORMAT}")
        key = key_of(row, f"{args.caller_audit_jsonl}:{line_no}")
        if key in audits:
            raise ValueError(f"duplicate caller subtree audit row: {key}")
        audits[key] = row

    class_vtables: dict[str, set[str]] = defaultdict(set)
    for line_no, row in enumerate(read_jsonl(args.catalog_jsonl), 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{args.catalog_jsonl}:{line_no}: expected {CATALOG_FORMAT}")
        class_name = row.get("class_name")
        vtable = row.get("receiver_vtable_id")
        if isinstance(class_name, str) and class_name and isinstance(vtable, str) and vtable:
            class_vtables[class_name].add(vtable)

    query_by_key: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, row in enumerate(queries, 1):
        if row.get("format") != QUERY_FORMAT:
            raise ValueError(f"{args.v9_queries_jsonl}:{line_no}: expected {QUERY_FORMAT}")
        key = key_of(row, f"{args.v9_queries_jsonl}:{line_no}")
        if key in query_by_key:
            raise ValueError(f"duplicate query: {key}")
        query_by_key[key] = dict(row)

    output: list[dict[str, Any]] = []
    details: list[dict[str, Any]] = []
    counts: Counter[str] = Counter()

    for line_no, original in enumerate(inference, 1):
        if original.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"{args.v9_inference_jsonl}:{line_no}: expected {INFERENCE_FORMAT}")
        row = dict(original)
        key = key_of(row, f"{args.v9_inference_jsonl}:{line_no}")
        plan = plans.get(key)
        audit = audits.get(key)

        if row.get("status") != "receiver_not_single_pointer" or plan is None or audit is None:
            output.append(row)
            continue

        counts["eligible"] += 1
        recovery: dict[str, Any] = {
            "format": RECOVERY_FORMAT,
            "source_id": key[0],
            "call_index": key[1],
            "accepted": False,
            "parameter_index": parse_decimal(plan.get("parameter_index"), -1),
            "expected_direct_caller_functions": parse_decimal(plan.get("expected_direct_caller_functions"), -1),
            "expected_matched_caller_calls": parse_decimal(plan.get("expected_matched_caller_calls"), -1),
            "candidate_value_classes": audit.get("candidate_value_classes"),
            "candidate_receiver_vtable_ids": [],
            "reasons": [],
            "affects_routing": False,
        }
        reasons: list[str] = recovery["reasons"]

        if (plan.get("source_entry") or "").lower() != source_entry(key[0]).lower():
            reasons.append("plan_source_entry_mismatch")
        if normalize_hex(plan.get("dispatch_slot_offset")) != normalize_hex(row.get("dispatch_slot_offset")):
            reasons.append("plan_slot_mismatch")
        expected_callers = recovery["expected_direct_caller_functions"]
        expected_calls = recovery["expected_matched_caller_calls"]
        if expected_callers < 0 or expected_calls <= 0:
            reasons.append("plan_caller_coverage_invalid")
        if audit.get("summary_status") != "complete_caller_argument_subtrees":
            reasons.append("caller_subtree_coverage_incomplete")
        if audit.get("status") != "all_caller_arguments_converge_one_value_class":
            reasons.append("caller_arguments_not_single_convergent_class")
        if audit.get("direct_caller_functions") != expected_callers:
            reasons.append("caller_function_count_mismatch")
        if audit.get("matched_caller_calls") != expected_calls:
            reasons.append("matched_caller_call_count_mismatch")

        classes = audit.get("candidate_value_classes")
        if not isinstance(classes, list) or len(classes) != 1 or not isinstance(classes[0], str) or not classes[0]:
            reasons.append("caller_value_class_not_single")
            receiver_class = None
        else:
            receiver_class = classes[0]

        calls = audit.get("calls")
        if not isinstance(calls, list) or len(calls) != expected_calls:
            reasons.append("caller_detail_count_mismatch")
        elif receiver_class is not None:
            for call in calls:
                if not isinstance(call, dict) or call.get("value_classes") != [receiver_class]:
                    reasons.append("caller_detail_not_convergent")
                    break

        if receiver_class is not None:
            vtables = sorted(class_vtables.get(receiver_class, set()))
            recovery["candidate_receiver_vtable_ids"] = vtables
            if len(vtables) != 1:
                reasons.append("caller_value_class_vtable_not_unique")
        else:
            vtables = []

        if not reasons:
            vtable = vtables[0]
            recovery["accepted"] = True
            counts["accepted"] += 1
            row["class_name"] = receiver_class
            row["class_match_kind"] = "caller_argument_value_subtree_exact_catalog_class_name"
            row["class_evidence_source"] = "complete_direct_caller_argument_value_subtree_convergence"
            row["candidate_receiver_class_names"] = [receiver_class]
            row["candidate_receiver_vtable_ids"] = [vtable]
            row["receiver_vtable_id"] = vtable
            row["status"] = "resolved_unique_vtable"
            row["caller_argument_subtree_recovery"] = recovery

            if key in query_by_key:
                raise ValueError(f"query already exists for newly recovered row: {key}")
            query_by_key[key] = {
                "format": QUERY_FORMAT,
                "source_id": key[0],
                "call_index": key[1],
                "call_address": row.get("call_address"),
                "receiver_vtable_id": vtable,
                "dispatch_slot_offset": normalize_hex(row.get("dispatch_slot_offset")),
                "receiver_class_name": receiver_class,
                "receiver_class_match_kind": row["class_match_kind"],
                "receiver_class_evidence_source": row["class_evidence_source"],
                "receiver_type_name": row.get("receiver_type_name"),
                "receiver_type_display": row.get("receiver_type_display"),
                "runtime_validation_status": row.get("runtime_validation_status", "runtime_unobserved"),
                "evidence_ref": f"{key[0]}#call={key[1]}",
                "affects_routing": False,
            }
        else:
            counts["blocked"] += 1
            row["caller_argument_subtree_recovery"] = recovery

        details.append(recovery)
        output.append(row)

    output_queries = [query_by_key[key] for key in sorted(query_by_key)]
    resolved_keys = {key_of(row, "output") for row in output if row.get("status") in RESOLVED}
    query_keys = set(query_by_key)
    if resolved_keys != query_keys:
        raise ValueError(
            f"final query/inference resolved-key mismatch: "
            f"missing={sorted(resolved_keys - query_keys)} extra={sorted(query_keys - resolved_keys)}"
        )

    write_jsonl(args.output, output)
    write_jsonl(args.queries_output, output_queries)
    if args.details_output is not None:
        write_jsonl(args.details_output, details)

    print("format: NEVERGONE.CallerArgumentSubtreeRecoveryFinalization/1")
    print("inference rows:", len(inference))
    print("input queries:", len(queries))
    print("eligible caller-subtree rows:", counts["eligible"])
    print("caller-subtree recoveries accepted:", counts["accepted"])
    print("caller-subtree recoveries blocked:", counts["blocked"])
    print("output queries:", len(output_queries))
    print("affects routing: no")
    print("output:", args.output)
    print("queries output:", args.queries_output)
    if args.details_output is not None:
        print("details output:", args.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
