#!/usr/bin/env python3
"""Report per-source coverage for ProductionClassifier/5.1 evidence.

This audit exists because aggregate external-evidence coverage can be 100% from
callgraph rows even when receiver/type evidence covers none of the training set.
It reports function coverage separately for callgraph, receiver types, and
receiver/vtable inference, plus receiver CALLIND row counts and resolution status.
"""
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
from pathlib import Path
from typing import Any, Iterable

FORMAT = "NEVERGONE.ProductionEvidenceCoverage/1"


def read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def load_trace(path: Path) -> tuple[set[str], dict[str, str], Counter]:
    ids: set[str] = set()
    source_to_id: dict[str, str] = {}
    programs: Counter = Counter()
    for row in read_jsonl(path):
        record_id = row.get("id")
        program = row.get("program")
        entry = row.get("source_entry")
        if not all(isinstance(x, str) and x for x in (record_id, program, entry)):
            continue
        ids.add(record_id)
        source_to_id[f"{program}:{entry}"] = record_id
        programs[program] += 1
    if not ids:
        raise ValueError(f"{path}: no usable trace identities")
    return ids, source_to_id, programs


def pct(n: int, d: int) -> float:
    return round(n / d, 6) if d else 0.0


def source_program(source_id: str) -> str:
    return source_id.split(":", 1)[0] if ":" in source_id else ""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("trace", type=Path)
    parser.add_argument("--callgraph-metrics", type=Path, action="append", default=[])
    parser.add_argument("--receiver-types", type=Path)
    parser.add_argument("--receiver-inference", type=Path)
    parser.add_argument("--receiver-program", default="libcocos2dcpp.so")
    args = parser.parse_args()

    dataset_ids, source_to_id, programs = load_trace(args.trace)
    receiver_eligible = {
        record_id
        for source_id, record_id in source_to_id.items()
        if source_program(source_id) == args.receiver_program
    }

    callgraph_ids: set[str] = set()
    callgraph_rows = 0
    callgraph_unmatched = 0
    for path in args.callgraph_metrics:
        for row in read_jsonl(path):
            callgraph_rows += 1
            record_id = row.get("id")
            if isinstance(record_id, str) and record_id in dataset_ids:
                callgraph_ids.add(record_id)
            else:
                callgraph_unmatched += 1

    receiver_ids: set[str] = set()
    receiver_calls = 0
    receiver_unmatched = 0
    if args.receiver_types is not None:
        with args.receiver_types.open("r", encoding="utf-8", newline="") as handle:
            for row in csv.DictReader(handle, delimiter="\t"):
                receiver_calls += 1
                source_id = row.get("source_id") or ""
                record_id = source_to_id.get(source_id)
                if record_id is None:
                    receiver_unmatched += 1
                else:
                    receiver_ids.add(record_id)

    inference_ids: set[str] = set()
    resolved_ids: set[str] = set()
    inference_calls = 0
    inference_unmatched = 0
    statuses: Counter = Counter()
    if args.receiver_inference is not None:
        for row in read_jsonl(args.receiver_inference):
            inference_calls += 1
            source_id = row.get("source_id")
            status = str(row.get("status") or "missing")
            statuses[status] += 1
            record_id = source_to_id.get(source_id) if isinstance(source_id, str) else None
            if record_id is None:
                inference_unmatched += 1
                continue
            inference_ids.add(record_id)
            if status in {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match"}:
                resolved_ids.add(record_id)

    payload = {
        "format": FORMAT,
        "dataset_functions": len(dataset_ids),
        "dataset_programs": dict(sorted(programs.items())),
        "receiver_program": args.receiver_program,
        "receiver_eligible_functions": len(receiver_eligible),
        "callgraph": {
            "rows": callgraph_rows,
            "matched_functions": len(callgraph_ids),
            "function_coverage": pct(len(callgraph_ids), len(dataset_ids)),
            "unmatched_rows": callgraph_unmatched,
        },
        "receiver_types": {
            "call_rows": receiver_calls,
            "matched_functions": len(receiver_ids),
            "eligible_function_coverage": pct(len(receiver_ids & receiver_eligible), len(receiver_eligible)),
            "dataset_function_coverage": pct(len(receiver_ids), len(dataset_ids)),
            "unmatched_rows": receiver_unmatched,
        },
        "receiver_inference": {
            "call_rows": inference_calls,
            "matched_functions": len(inference_ids),
            "resolved_functions": len(resolved_ids),
            "eligible_function_coverage": pct(len(inference_ids & receiver_eligible), len(receiver_eligible)),
            "resolved_eligible_function_coverage": pct(len(resolved_ids & receiver_eligible), len(receiver_eligible)),
            "unmatched_rows": inference_unmatched,
            "status_counts": dict(statuses.most_common()),
        },
    }

    print("format:", FORMAT)
    print("dataset functions:", payload["dataset_functions"])
    print("dataset programs:", payload["dataset_programs"])
    print("receiver eligible functions:", payload["receiver_eligible_functions"])
    print("callgraph coverage:", json.dumps(payload["callgraph"], sort_keys=True))
    print("receiver type coverage:", json.dumps(payload["receiver_types"], sort_keys=True))
    print("receiver inference coverage:", json.dumps(payload["receiver_inference"], sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
