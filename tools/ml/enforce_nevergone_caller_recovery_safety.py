#!/usr/bin/env python3
"""Fail-closed finalization for caller-derived receiver-vtable recoveries.

Consumes receiver inference, its query set, and the independent resolved-caller
safety audit. Caller-derived resolved rows survive only when a matching audit row
is explicitly safe. Unsafe or missing evidence is demoted and its query removed.
No new receiver class or vtable is inferred here.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
QUERY_FORMAT = "NEVERGONE.StaticReceiverVtableQuery/1"
AUDIT_FORMAT = "NEVERGONE.ResolvedCallerSafetyAudit/1"
CALLER_SOURCES = {"parameter_caller_type", "structural_parameter_caller_type"}
RESOLVED = {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match"}


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


def key_of(row: dict[str, Any], *, where: str) -> tuple[str, int]:
    source_id = row.get("source_id")
    call_index = row.get("call_index")
    if not isinstance(source_id, str) or not source_id or not isinstance(call_index, int) or call_index < 0:
        raise ValueError(f"{where}: invalid source_id/call_index")
    return source_id, call_index


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("queries_jsonl", type=Path)
    parser.add_argument("caller_safety_audit_jsonl", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--queries-output", type=Path, required=True)
    args = parser.parse_args()

    inference = read_jsonl(args.inference_jsonl)
    queries = read_jsonl(args.queries_jsonl)
    audit_rows = read_jsonl(args.caller_safety_audit_jsonl)

    audit_by_key: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, row in enumerate(audit_rows, 1):
        if row.get("format") != AUDIT_FORMAT:
            raise ValueError(f"{args.caller_safety_audit_jsonl}:{line_no}: expected {AUDIT_FORMAT}")
        key = key_of(row, where=f"{args.caller_safety_audit_jsonl}:{line_no}")
        if key in audit_by_key:
            raise ValueError(f"duplicate caller safety audit row: {key[0]}#{key[1]}")
        audit_by_key[key] = row

    query_by_key: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, row in enumerate(queries, 1):
        if row.get("format") != QUERY_FORMAT:
            raise ValueError(f"{args.queries_jsonl}:{line_no}: expected {QUERY_FORMAT}")
        key = key_of(row, where=f"{args.queries_jsonl}:{line_no}")
        if key in query_by_key:
            raise ValueError(f"duplicate query row: {key[0]}#{key[1]}")
        query_by_key[key] = row

    output: list[dict[str, Any]] = []
    accepted_query_keys: set[tuple[str, int]] = set()
    counts: Counter[str] = Counter()

    for line_no, original in enumerate(inference, 1):
        if original.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"{args.inference_jsonl}:{line_no}: expected {INFERENCE_FORMAT}")
        row = dict(original)
        key = key_of(row, where=f"{args.inference_jsonl}:{line_no}")
        status = row.get("status")
        source = row.get("class_evidence_source")

        caller_resolved = status in RESOLVED and source in CALLER_SOURCES
        if caller_resolved:
            counts["caller_derived_resolved"] += 1
            audit = audit_by_key.get(key)
            reasons: list[str] = []
            if audit is None:
                reasons.append("missing_caller_safety_audit")
            else:
                if audit.get("status") != "safe_caller_recovery":
                    reasons.append("caller_safety_audit_not_safe")
                if audit.get("call_address") != row.get("call_address"):
                    reasons.append("caller_safety_call_address_mismatch")
                if audit.get("class_evidence_source") != source:
                    reasons.append("caller_safety_source_mismatch")
                if audit.get("inferred_class_name") != row.get("class_name"):
                    reasons.append("caller_safety_class_mismatch")
                if source == "structural_parameter_caller_type" and audit.get("structural_slot_match") is not True:
                    reasons.append("caller_safety_structural_slot_not_verified")
                audit_reasons = audit.get("reasons")
                if isinstance(audit_reasons, list) and audit_reasons:
                    reasons.append("caller_safety_audit_has_reasons")

            row["caller_safety_audit_present"] = audit is not None
            row["caller_safety_status"] = audit.get("status") if audit else None
            row["caller_safety_reasons"] = reasons
            if reasons:
                counts["caller_safety_blocked"] += 1
                row["blocked_status"] = status
                row["blocked_receiver_vtable_id"] = row.get("receiver_vtable_id")
                row["receiver_vtable_id"] = None
                row["status"] = "caller_provenance_safety_blocked"
            else:
                counts["caller_safety_accepted"] += 1
                accepted_query_keys.add(key)
        elif status in RESOLVED:
            accepted_query_keys.add(key)

        output.append(row)

    output_queries: list[dict[str, Any]] = []
    dropped_queries = 0
    for key, query in query_by_key.items():
        if key in accepted_query_keys:
            output_queries.append(query)
        else:
            dropped_queries += 1

    resolved_output_keys = {
        key_of(row, where="output") for row in output if row.get("status") in RESOLVED
    }
    query_output_keys = {key_of(row, where="output query") for row in output_queries}
    if query_output_keys != resolved_output_keys:
        missing = sorted(resolved_output_keys - query_output_keys)
        extra = sorted(query_output_keys - resolved_output_keys)
        raise ValueError(f"final query/inference resolved-key mismatch: missing={missing} extra={extra}")

    write_jsonl(args.output, output)
    write_jsonl(args.queries_output, output_queries)

    print("format: NEVERGONE.CallerRecoverySafetyFinalization/1")
    print("inference rows:", len(inference))
    print("caller safety audit rows:", len(audit_rows))
    print("caller-derived resolved rows:", counts["caller_derived_resolved"])
    print("caller safety accepted:", counts["caller_safety_accepted"])
    print("caller safety blocked:", counts["caller_safety_blocked"])
    print("input queries:", len(queries))
    print("output queries:", len(output_queries))
    print("queries dropped by safety gate:", dropped_queries)
    print("affects routing: no")
    print("output:", args.output)
    print("queries output:", args.queries_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
