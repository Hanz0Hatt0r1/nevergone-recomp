#!/usr/bin/env python3
"""Build a bounded unidbg runtime CALLIND trace plan from seed-template rows.

Only call sites that belong to repeated exact ordered dispatch groups are selected.
The output is a JSON array consumed by DispatchTargetRuntimeTrace; it contains no
runtime targets and does not modify routing.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.DispatchRuntimeProbePlan/1"


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


def parse_call_address(value: Any) -> int:
    if not isinstance(value, str) or not value:
        raise ValueError(f"invalid call_address: {value!r}")
    token = value.lower()
    if token.startswith("0x"):
        token = token[2:]
    return int(token, 16)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("seed_template", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--min-group-size", type=int, default=2)
    parser.add_argument("--max-sites", type=int, default=128)
    args = parser.parse_args()
    if args.min_group_size < 2:
        raise SystemExit("--min-group-size must be >= 2")
    if args.max_sites < 1 or args.max_sites > 128:
        raise SystemExit("--max-sites must be 1..128")

    rows = read_jsonl(args.seed_template)
    selected: list[dict[str, Any]] = []
    seen: set[tuple[str, int]] = set()
    for row in rows:
        group_size = row.get("ordered_group_size")
        if not isinstance(group_size, int) or group_size < args.min_group_size:
            continue
        source_id = row.get("source_id")
        call_index = row.get("call_index")
        if not isinstance(source_id, str) or not source_id or not isinstance(call_index, int):
            raise ValueError("seed-template row missing source_id/call_index")
        key = (source_id, call_index)
        if key in seen:
            continue
        seen.add(key)
        call_address = row.get("call_address")
        offset = parse_call_address(call_address)
        selected.append(
            {
                "format": FORMAT,
                "source_id": source_id,
                "call_index": call_index,
                "call_address": call_address,
                "call_site_offset": f"0x{offset:x}",
                "slot_group_id": row.get("slot_group_id"),
                "ordered_group_id": row.get("ordered_group_id"),
                "ordered_position": row.get("ordered_position"),
                "ordered_group_size": group_size,
            }
        )

    selected.sort(
        key=lambda row: (
            -int(row["ordered_group_size"]),
            str(row.get("ordered_group_id") or ""),
            int(row.get("ordered_position") or 0),
            row["source_id"],
            row["call_index"],
        )
    )
    if not selected:
        raise SystemExit("no repeated exact ordered dispatch sites selected")
    if len(selected) > args.max_sites:
        raise SystemExit(f"selected {len(selected)} sites, exceeds --max-sites={args.max_sites}")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(selected, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    groups = Counter(str(row.get("ordered_group_id")) for row in selected)
    print("format:", FORMAT)
    print("selected sites:", len(selected))
    print("ordered groups:", len(groups))
    print("group sizes:", dict(sorted(groups.items())))
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
