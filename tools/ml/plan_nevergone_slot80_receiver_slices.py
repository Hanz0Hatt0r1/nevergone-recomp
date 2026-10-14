#!/usr/bin/env python3
"""Plan bounded, evidence-only Ghidra slices for still-unresolved v11 slot 0x80 calls."""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any

INF_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
PLAN_FORMAT = "NEVERGONE.Slot80ReceiverSlicePlan/1"
ELIGIBLE = {"unresolved_receiver_class", "ambiguous_receiver_class", "receiver_not_single_pointer"}
SOURCE = re.compile(r"libcocos2dcpp\.so:([0-9a-f]{8})\Z")
FIELDS = ("format", "source_id", "source_entry", "call_indices", "call_addresses", "expected_calls")


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    with path.open(encoding="utf-8") as handle:
        rows = [json.loads(line) for line in handle if line.strip()]
    if any(not isinstance(row, dict) for row in rows):
        raise ValueError(f"{path}: expected JSON objects")
    return rows


def selected_calls(failures: Path, inference: Path) -> dict[str, list[dict[str, Any]]]:
    inf_by_key: dict[tuple[str, int], dict[str, Any]] = {}
    for row in read_jsonl(inference):
        if row.get("format") != INF_FORMAT:
            raise ValueError("inference format mismatch")
        key = (row.get("source_id"), row.get("call_index"))
        if key in inf_by_key:
            raise ValueError(f"duplicate inference key: {key}")
        inf_by_key[key] = row

    seen: set[tuple[str, int]] = set()
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in read_jsonl(failures):
        source = row.get("source_id")
        call = row.get("call_index")
        key = (source, call)
        if not isinstance(source, str) or not isinstance(call, int) or call < 0:
            raise ValueError(f"invalid failure key: {key}")
        if key in seen:
            raise ValueError(f"duplicate failure key: {key}")
        seen.add(key)
        if row.get("dispatch_slot_offset") != "0x80" or row.get("status") not in ELIGIBLE:
            continue
        if SOURCE.fullmatch(source) is None:
            raise ValueError(f"unexpected source identity: {source}")
        matched = inf_by_key.get(key)
        if matched is None or any(matched.get(k) != row.get(k) for k in (
            "status", "dispatch_slot_offset", "call_address")):
            raise ValueError(f"v11 inference/failure disagreement: {key}")
        address = row.get("call_address")
        if not isinstance(address, str) or re.fullmatch(r"[0-9a-f]{8}", address) is None:
            raise ValueError(f"invalid call address: {key} {address!r}")
        grouped[source].append(row)
    if not grouped:
        raise ValueError("no unresolved v11 slot 0x80 calls")
    return grouped


def make_plan(grouped: dict[str, list[dict[str, Any]]]) -> list[dict[str, str]]:
    plan: list[dict[str, str]] = []
    for source, rows in sorted(grouped.items()):
        rows = sorted(rows, key=lambda r: r["call_index"])
        plan.append({
            "format": PLAN_FORMAT,
            "source_id": source,
            "source_entry": source.split(":", 1)[1],
            "call_indices": ",".join(str(row["call_index"]) for row in rows),
            "call_addresses": ",".join(row["call_address"] for row in rows),
            "expected_calls": str(len(rows)),
        })
    return plan


def write_plan(path: Path, rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDS, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("failures_jsonl", type=Path)
    p.add_argument("inference_jsonl", type=Path)
    p.add_argument("--output", required=True, type=Path)
    p.add_argument("--expected-calls", type=int)
    p.add_argument("--expected-sources", type=int)
    a = p.parse_args()
    plan = make_plan(selected_calls(a.failures_jsonl, a.inference_jsonl))
    calls = sum(int(r["expected_calls"]) for r in plan)
    if a.expected_calls is not None and calls != a.expected_calls:
        raise ValueError(f"expected {a.expected_calls} calls, found {calls}")
    if a.expected_sources is not None and len(plan) != a.expected_sources:
        raise ValueError(f"expected {a.expected_sources} sources, found {len(plan)}")
    write_plan(a.output, plan)
    print("format:", PLAN_FORMAT)
    print("unresolved slot 0x80 calls:", calls)
    print("source functions:", len(plan))
    for row in plan:
        print(row["source_id"], "calls", row["call_indices"])
    print("output:", a.output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
