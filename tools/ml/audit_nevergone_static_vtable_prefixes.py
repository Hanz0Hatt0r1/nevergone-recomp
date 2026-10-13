#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import math
from collections import Counter, defaultdict
from pathlib import Path
from statistics import mean, median
from typing import Any

EXPECTED_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
POINTER_SIZE = 4


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            if row.get("format") != EXPECTED_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {EXPECTED_FORMAT}")
            rows.append(row)
    return rows


def parse_hex(value: Any) -> int | None:
    if not isinstance(value, str):
        return None
    try:
        return int(value, 0)
    except ValueError:
        return None


def percentile(values: list[int], q: float) -> int:
    if not values:
        return 0
    ordered = sorted(values)
    index = max(0, min(len(ordered) - 1, math.ceil(q * len(ordered)) - 1))
    return ordered[index]


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit conservative primary-vtable prefix evidence")
    parser.add_argument("catalog", type=Path)
    parser.add_argument("--largest-output", type=Path)
    parser.add_argument("--top", type=int, default=30)
    args = parser.parse_args()

    rows = read_jsonl(args.catalog)
    by_vtable: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        by_vtable[str(row["receiver_vtable_id"])].append(row)

    prefix_sizes: list[int] = []
    target_reuse: Counter[str] = Counter()
    class_named = 0
    named_targets = 0
    utilization: list[float] = []
    largest: list[dict[str, Any]] = []

    for vtable_id, items in by_vtable.items():
        items.sort(key=lambda x: int(str(x["dispatch_slot_offset"]), 0))
        class_name = str(items[0].get("class_name") or "")
        prefix_count = int(items[0].get("primary_prefix_slot_count") or len(items))
        prefix_sizes.append(prefix_count)

        for row in items:
            for target_id in row.get("candidate_target_ids") or []:
                target_reuse[str(target_id)] += 1
            for target in row.get("target_functions") or []:
                name = target.get("name")
                if isinstance(name, str) and name:
                    named_targets += 1
                    if class_name and name.startswith(class_name + "::"):
                        class_named += 1

        address_point = parse_hex(items[0].get("receiver_vtable_address_point"))
        next_starts = [
            parse_hex(x) for x in (items[0].get("next_vtable_symbol_starts") or [])
        ]
        next_starts = [x for x in next_starts if x is not None]
        util = None
        if address_point is not None and next_starts:
            next_start = min(next_starts)
            available = next_start - address_point
            used = prefix_count * POINTER_SIZE
            if available > 0:
                util = used / available
                utilization.append(util)

        largest.append({
            "receiver_vtable_id": vtable_id,
            "class_name": class_name,
            "primary_prefix_slot_count": prefix_count,
            "unique_targets_in_vtable": len({
                t for row in items for t in (row.get("candidate_target_ids") or [])
            }),
            "next_vtable_utilization": util,
            "runtime_matches": sum(
                row.get("runtime_validation_status") == "runtime_match" for row in items
            ),
            "runtime_mismatches": sum(
                row.get("runtime_validation_status") == "runtime_mismatch" for row in items
            ),
        })

    largest.sort(key=lambda x: (-int(x["primary_prefix_slot_count"]), x["receiver_vtable_id"]))
    unique_targets = len(target_reuse)
    reused_targets = sum(count > 1 for count in target_reuse.values())
    slots_using_reused_targets = sum(count for count in target_reuse.values() if count > 1)

    print("format: NEVERGONE.StaticVtablePrefixAudit/1")
    print("catalog rows:", len(rows))
    print("vtables:", len(by_vtable))
    print("unique target functions:", unique_targets)
    print("slot/unique-target ratio:", round(len(rows) / unique_targets, 6) if unique_targets else 0.0)
    print("targets reused by multiple vtables/slots:", reused_targets)
    print("slots backed by reused targets:", slots_using_reused_targets)
    print("prefix slots mean:", round(mean(prefix_sizes), 6) if prefix_sizes else 0.0)
    print("prefix slots median:", median(prefix_sizes) if prefix_sizes else 0)
    print("prefix slots p90:", percentile(prefix_sizes, 0.90))
    print("prefix slots p95:", percentile(prefix_sizes, 0.95))
    print("prefix slots p99:", percentile(prefix_sizes, 0.99))
    print("prefix slots max:", max(prefix_sizes) if prefix_sizes else 0)
    print("named targets:", named_targets)
    print("same-class named targets:", class_named)
    print("same-class named target fraction:", round(class_named / named_targets, 6) if named_targets else 0.0)
    print("vtable spans with next-symbol utilization:", len(utilization))
    if utilization:
        util_sorted = sorted(utilization)
        print("next-symbol utilization median:", round(util_sorted[len(util_sorted)//2], 6))
        print("next-symbol utilization p90:", round(util_sorted[max(0, math.ceil(0.90*len(util_sorted))-1)], 6))
        print("next-symbol utilization max:", round(max(util_sorted), 6))
    print("runtime matches:", sum(row.get("runtime_validation_status") == "runtime_match" for row in rows))
    print("runtime mismatches:", sum(row.get("runtime_validation_status") == "runtime_mismatch" for row in rows))

    print("\nlargest primary prefixes:")
    for item in largest[: max(0, args.top)]:
        util = item["next_vtable_utilization"]
        util_text = "-" if util is None else f"{util:.3f}"
        print(
            f"{item['primary_prefix_slot_count']:4} slots  util={util_text:>6}  "
            f"matches={item['runtime_matches']} mismatches={item['runtime_mismatches']}  "
            f"{item['receiver_vtable_id']}  {item['class_name']}"
        )

    if args.largest_output is not None:
        args.largest_output.parent.mkdir(parents=True, exist_ok=True)
        with args.largest_output.open("w", encoding="utf-8") as handle:
            for item in largest:
                handle.write(json.dumps(item, sort_keys=True) + "\n")
        print("largest output:", args.largest_output)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
