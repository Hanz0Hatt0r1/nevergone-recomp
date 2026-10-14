#!/usr/bin/env python3
"""Build a narrow verification plan for caller-derived receiver recoveries."""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
CALLER_SOURCES = {
    "parameter_caller_type": "parameter",
    "structural_parameter_caller_type": "structural_parameter",
}
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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    rows = read_jsonl(args.inference_jsonl)
    plan: list[dict[str, str]] = []
    for row in rows:
        if row.get("format") != FORMAT:
            raise ValueError(f"expected {FORMAT}")
        source_kind = row.get("class_evidence_source")
        if row.get("status") not in RESOLVED or source_kind not in CALLER_SOURCES:
            continue
        source_id = row.get("source_id")
        source_entry = row.get("source_entry")
        call_index = row.get("call_index")
        class_name = row.get("class_name")
        slot = row.get("dispatch_slot_offset")
        if not isinstance(source_id, str) or not isinstance(source_entry, str):
            continue
        if not isinstance(call_index, int) or not isinstance(class_name, str):
            continue
        if source_kind == "parameter_caller_type":
            param_index = row.get("parameter_index")
        else:
            param_index = row.get("structural_parameter_index")
        if not isinstance(param_index, int) or param_index < 0:
            raise ValueError(f"{source_id}#{call_index}: caller-derived row missing parameter index")
        plan.append({
            "source_id": source_id,
            "source_entry": source_entry,
            "call_index": str(call_index),
            "parameter_index": str(param_index),
            "evidence_source": CALLER_SOURCES[source_kind],
            "inferred_class_name": class_name,
            "dispatch_slot_offset": str(slot or "-"),
        })

    plan.sort(key=lambda row: (row["source_id"], int(row["call_index"])))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "source_id", "source_entry", "call_index", "parameter_index",
        "evidence_source", "inferred_class_name", "dispatch_slot_offset",
    ]
    with args.output.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t")
        writer.writeheader()
        writer.writerows(plan)

    direct = sum(row["evidence_source"] == "parameter" for row in plan)
    structural = sum(row["evidence_source"] == "structural_parameter" for row in plan)
    print("format: NEVERGONE.ResolvedCallerVerificationPlan/1")
    print("inference rows:", len(rows))
    print("caller-derived resolved rows:", len(plan))
    print("direct parameter caller rows:", direct)
    print("structural parameter caller rows:", structural)
    print("output:", args.output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
