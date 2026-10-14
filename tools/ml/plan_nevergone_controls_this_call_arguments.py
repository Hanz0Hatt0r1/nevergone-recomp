#!/usr/bin/env python3
"""Plan all Controls call arguments previously classified as parameter-zero-derived."""
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
from pathlib import Path

FORMAT = "NEVERGONE.ControlsThisCallArgumentPlan/1"
SOURCE_FORMAT = "NEVERGONE.ControlsFieldAliasTriage/1"
EXPECTED_CATEGORIES = {
    "callback_factory_candidate": 1,
    "ccpoint_subobject_candidate": 43,
    "cocos_base_method_candidate": 6,
    "controls_method_candidate": 21,
    "controlsbutton_factory_callback_candidate": 3,
    "indirect_unresolved": 3,
    "operator_delete_candidate": 1,
}
EXPECTED_ARGUMENT_INDICES = {"0": 57, "1": 18, "2": 3}


def read_jsonl(path: Path) -> list[dict]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("triage_jsonl", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    rows = read_jsonl(args.triage_jsonl)
    if not rows:
        raise ValueError("empty Controls alias triage")
    summary = rows[0]
    if (
        summary.get("format") != SOURCE_FORMAT
        or summary.get("record_type") != "summary"
        or summary.get("this_bearing_call_count") != 78
        or summary.get("callee_categories") != EXPECTED_CATEGORIES
    ):
        raise ValueError("Controls alias triage summary mismatch")

    calls = [row for row in rows if row.get("record_type") == "this_bearing_call_candidate"]
    if len(calls) != 78:
        raise ValueError("expected 78 this-bearing call candidates")

    seen: set[tuple[str, str, str]] = set()
    output: list[dict[str, str]] = []
    for row in calls:
        argument_index = row.get("this_argument_indices", "")
        if not isinstance(argument_index, str) or not argument_index.isdigit():
            raise ValueError("expected exactly one this-derived argument index")
        key = (row["function_entry"], row["op_address"], row["op_time"])
        if key in seen:
            raise ValueError("duplicate this-bearing call site: " + ":".join(key))
        seen.add(key)
        output.append(
            {
                "format": FORMAT,
                "function_entry": row["function_entry"],
                "op_address": row["op_address"],
                "op_time": row["op_time"],
                "call_kind": row["call_kind"],
                "call_target_token": row["call_target_token"],
                "callee_category": row["callee_category"],
                "callee_symbol": row.get("callee_symbol") or "-",
                "argument_index": argument_index,
            }
        )

    if Counter(row["callee_category"] for row in output) != Counter(EXPECTED_CATEGORIES):
        raise ValueError("callee category counts changed")
    if Counter(row["argument_index"] for row in output) != Counter(EXPECTED_ARGUMENT_INDICES):
        raise ValueError("argument-index distribution changed")

    output.sort(key=lambda row: (row["function_entry"], row["op_address"], int(row["op_time"])))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(output[0]), delimiter="\t")
        writer.writeheader()
        writer.writerows(output)

    print("format:", FORMAT)
    print("calls:", len(output))
    print("argument indices:", dict(sorted(Counter(row["argument_index"] for row in output).items())))
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
