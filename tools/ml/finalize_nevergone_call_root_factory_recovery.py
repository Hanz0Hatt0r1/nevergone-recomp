#!/usr/bin/env python3
"""Resolve v8 CALL-root receivers only from constructor-proven factory returns.

The post-finalizer is fail-closed. It accepts a row only when the current v8
inference still reports receiver_not_single_pointer, the persisted CALL-root
plan matches that row's call/slot and callee, and the independent factory-return
audit reports exactly one catalog class and exactly one vtable with status
unique_factory_return_class. No function-name, sibling, neighbor, or target-slot
class inference is introduced.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
QUERY_FORMAT = "NEVERGONE.StaticReceiverVtableQuery/1"
AUDIT_FORMAT = "NEVERGONE.CallRootFactoryReturnAudit/1"
RECOVERY_FORMAT = "NEVERGONE.CallRootFactoryRecovery/1"
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


def normalize_address(value: Any) -> str | None:
    norm = normalize_hex(value)
    if norm is None:
        return None
    return f"{int(norm, 0):08x}"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("v8_inference_jsonl", type=Path)
    parser.add_argument("v8_queries_jsonl", type=Path)
    parser.add_argument("factory_plan_tsv", type=Path)
    parser.add_argument("factory_audit_jsonl", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--queries-output", type=Path, required=True)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    inference = read_jsonl(args.v8_inference_jsonl)
    queries = read_jsonl(args.v8_queries_jsonl)

    plans: dict[tuple[str, int], dict[str, str]] = {}
    for line_no, row in enumerate(read_tsv(args.factory_plan_tsv), 2):
        source_id = row.get("source_id") or ""
        call_index = parse_decimal(row.get("call_index"), -1)
        if not source_id or call_index < 0:
            raise ValueError(f"{args.factory_plan_tsv}:{line_no}: invalid source/call")
        key = (source_id, call_index)
        if key in plans:
            raise ValueError(f"duplicate CALL-root plan row: {key}")
        plans[key] = row

    audits: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, row in enumerate(read_jsonl(args.factory_audit_jsonl), 1):
        if row.get("format") != AUDIT_FORMAT:
            raise ValueError(f"{args.factory_audit_jsonl}:{line_no}: expected {AUDIT_FORMAT}")
        key = key_of(row, f"{args.factory_audit_jsonl}:{line_no}")
        if key in audits:
            raise ValueError(f"duplicate factory audit row: {key}")
        audits[key] = row

    query_by_key: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, row in enumerate(queries, 1):
        if row.get("format") != QUERY_FORMAT:
            raise ValueError(f"{args.v8_queries_jsonl}:{line_no}: expected {QUERY_FORMAT}")
        key = key_of(row, f"{args.v8_queries_jsonl}:{line_no}")
        if key in query_by_key:
            raise ValueError(f"duplicate query: {key}")
        query_by_key[key] = dict(row)

    output: list[dict[str, Any]] = []
    details: list[dict[str, Any]] = []
    counts: Counter[str] = Counter()

    for line_no, original in enumerate(inference, 1):
        if original.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"{args.v8_inference_jsonl}:{line_no}: expected {INFERENCE_FORMAT}")
        row = dict(original)
        key = key_of(row, f"{args.v8_inference_jsonl}:{line_no}")
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
            "callee_entry": audit.get("callee_entry"),
            "callee_name": audit.get("callee_name"),
            "candidate_receiver_class_names": audit.get("candidate_receiver_class_names"),
            "candidate_receiver_vtable_ids": audit.get("candidate_receiver_vtable_ids"),
            "reasons": [],
            "affects_routing": False,
        }
        reasons: list[str] = recovery["reasons"]

        if normalize_address(plan.get("call_address")) != normalize_address(row.get("call_address")):
            reasons.append("plan_call_address_mismatch")
        if normalize_hex(plan.get("dispatch_slot_offset")) != normalize_hex(row.get("dispatch_slot_offset")):
            reasons.append("plan_slot_mismatch")
        if normalize_address(plan.get("callee_entry")) != normalize_address(audit.get("callee_entry")):
            reasons.append("plan_callee_entry_mismatch")
        if (plan.get("callee_name") or "-") != (audit.get("callee_name") or "-"):
            reasons.append("plan_callee_name_mismatch")
        if audit.get("status") != "unique_factory_return_class":
            reasons.append("factory_audit_not_unique")

        classes = audit.get("candidate_receiver_class_names")
        vtables = audit.get("candidate_receiver_vtable_ids")
        if not isinstance(classes, list) or len(classes) != 1 or not isinstance(classes[0], str) or not classes[0]:
            reasons.append("factory_class_not_single")
        if not isinstance(vtables, list) or len(vtables) != 1 or not isinstance(vtables[0], str) or not vtables[0]:
            reasons.append("factory_vtable_not_single")

        if not reasons:
            receiver_class = classes[0]
            vtable = vtables[0]
            recovery["accepted"] = True
            counts["accepted"] += 1
            row["class_name"] = receiver_class
            row["class_match_kind"] = "factory_return_constructor_exact_catalog_class_name"
            row["class_evidence_source"] = "call_root_factory_return_constructor_provenance"
            row["candidate_receiver_class_names"] = [receiver_class]
            row["candidate_receiver_vtable_ids"] = [vtable]
            row["receiver_vtable_id"] = vtable
            row["status"] = "resolved_unique_vtable"
            row["call_root_factory_recovery"] = recovery

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
            row["call_root_factory_recovery"] = recovery

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

    print("format: NEVERGONE.CallRootFactoryRecoveryFinalization/1")
    print("inference rows:", len(inference))
    print("input queries:", len(queries))
    print("eligible CALL-root rows:", counts["eligible"])
    print("CALL-root factory recoveries accepted:", counts["accepted"])
    print("CALL-root factory recoveries blocked:", counts["blocked"])
    print("output queries:", len(output_queries))
    print("affects routing: no")
    print("output:", args.output)
    print("queries output:", args.queries_output)
    if args.details_output is not None:
        print("details output:", args.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
