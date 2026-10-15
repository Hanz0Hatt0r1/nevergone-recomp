#!/usr/bin/env python3
"""Plan structural receiver slices for the post-v17 non-slot-0x80 frontier.

The planner is deliberately evidence-only. It consumes the exact failure audit and
matching inference output produced after v17, requires the slot-0x80 frontier to be
closed, and selects only still-unresolved receiver-class rows on other slots.
Nothing here infers a class or changes routing.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any

INF_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
PLAN_FORMAT = "NEVERGONE.NonSlot80ReceiverFrontierPlan/1"
EXPECTED_STATUS = "unresolved_receiver_class"
SOURCE_RE = re.compile(r"libcocos2dcpp\.so:([0-9a-f]{8})\Z")
ADDRESS_RE = re.compile(r"[0-9a-f]{8}\Z")
FIELDS = (
    "format",
    "source_id",
    "source_entry",
    "call_indices",
    "call_addresses",
    "slot_offsets",
    "expected_calls",
)


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open(encoding="utf-8") as handle:
        for line_no, line in enumerate(handle, 1):
            if not line.strip():
                continue
            row = json.loads(line)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            rows.append(row)
    return rows


def normalize_slot(value: Any) -> str | None:
    if value in (None, "", "-"):
        return None
    try:
        parsed = int(str(value), 0)
    except (TypeError, ValueError):
        return None
    return f"0x{parsed:x}" if parsed >= 0 else None


def inference_by_key(path: Path) -> dict[tuple[str, int], dict[str, Any]]:
    result: dict[tuple[str, int], dict[str, Any]] = {}
    for line_no, row in enumerate(read_jsonl(path), 1):
        if row.get("format") != INF_FORMAT:
            raise ValueError(f"{path}:{line_no}: inference format mismatch")
        source = row.get("source_id")
        call = row.get("call_index")
        if not isinstance(source, str) or not isinstance(call, int) or call < 0:
            raise ValueError(f"{path}:{line_no}: invalid inference identity")
        key = (source, call)
        if key in result:
            raise ValueError(f"duplicate inference key: {key}")
        result[key] = row
    return result


def selected_calls(
    failures_path: Path,
    inference_path: Path,
) -> dict[str, list[dict[str, Any]]]:
    inference = inference_by_key(inference_path)
    failures = read_jsonl(failures_path)
    if not failures:
        raise ValueError("failure audit is empty; v18 expects a residual frontier")

    selected: dict[str, list[dict[str, Any]]] = defaultdict(list)
    seen: set[tuple[str, int]] = set()

    for line_no, row in enumerate(failures, 1):
        source = row.get("source_id")
        call = row.get("call_index")
        key = (source, call)
        if not isinstance(source, str) or SOURCE_RE.fullmatch(source) is None:
            raise ValueError(f"{failures_path}:{line_no}: unexpected source identity: {source!r}")
        if not isinstance(call, int) or call < 0:
            raise ValueError(f"{failures_path}:{line_no}: invalid call index")
        if key in seen:
            raise ValueError(f"duplicate failure key: {key}")
        seen.add(key)

        matched = inference.get(key)
        if matched is None:
            raise ValueError(f"failure missing matching inference row: {key}")
        for field in ("status", "call_address"):
            if matched.get(field) != row.get(field):
                raise ValueError(f"inference/failure {field} disagreement: {key}")
        slot = normalize_slot(row.get("dispatch_slot_offset"))
        inf_slot = normalize_slot(matched.get("dispatch_slot_offset"))
        if slot is None or inf_slot != slot:
            raise ValueError(f"inference/failure slot disagreement: {key}")

        address = row.get("call_address")
        if not isinstance(address, str) or ADDRESS_RE.fullmatch(address) is None:
            raise ValueError(f"invalid call address for {key}: {address!r}")

        # v18 starts only after v17 has closed the entire slot-0x80 frontier.
        if slot == "0x80":
            raise ValueError(f"post-v17 slot-0x80 failure remains unresolved: {key}")

        # The expected post-v17 frontier is class-resolution only. A new failure
        # shape means the topology changed and must be reviewed, not silently mixed
        # into this probe.
        if row.get("status") != EXPECTED_STATUS:
            raise ValueError(
                f"unexpected post-v17 non-slot80 failure status for {key}: "
                f"{row.get('status')!r}"
            )
        if row.get("candidate_receiver_class_names") not in (None, []):
            raise ValueError(f"unresolved-class row unexpectedly has class candidates: {key}")

        selected[source].append(dict(row, dispatch_slot_offset=slot))

    if not selected:
        raise ValueError("no post-v17 non-slot80 unresolved receiver-class rows")
    return selected


def make_plan(grouped: dict[str, list[dict[str, Any]]]) -> list[dict[str, str]]:
    plan: list[dict[str, str]] = []
    for source, rows in sorted(grouped.items()):
        rows = sorted(rows, key=lambda row: row["call_index"])
        plan.append(
            {
                "format": PLAN_FORMAT,
                "source_id": source,
                "source_entry": source.split(":", 1)[1],
                "call_indices": ",".join(str(row["call_index"]) for row in rows),
                "call_addresses": ",".join(str(row["call_address"]) for row in rows),
                "slot_offsets": ",".join(str(row["dispatch_slot_offset"]) for row in rows),
                "expected_calls": str(len(rows)),
            }
        )
    return plan


def write_plan(path: Path, rows: list[dict[str, str]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=FIELDS, delimiter="\t")
        writer.writeheader()
        writer.writerows(rows)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("failures_jsonl", type=Path)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--expected-calls", type=int)
    parser.add_argument("--expected-sources", type=int)
    args = parser.parse_args()

    grouped = selected_calls(args.failures_jsonl, args.inference_jsonl)
    plan = make_plan(grouped)
    call_count = sum(int(row["expected_calls"]) for row in plan)
    if args.expected_calls is not None and call_count != args.expected_calls:
        raise ValueError(f"expected {args.expected_calls} residual calls, found {call_count}")
    if args.expected_sources is not None and len(plan) != args.expected_sources:
        raise ValueError(f"expected {args.expected_sources} source functions, found {len(plan)}")

    write_plan(args.output, plan)
    print("format:", PLAN_FORMAT)
    print("post-v17 non-slot80 unresolved calls:", call_count)
    print("source functions:", len(plan))
    for row in plan:
        print(
            row["source_id"],
            "calls", row["call_indices"],
            "slots", row["slot_offsets"],
        )
    print("output:", args.output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
