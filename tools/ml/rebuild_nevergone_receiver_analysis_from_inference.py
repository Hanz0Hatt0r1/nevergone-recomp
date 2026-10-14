#!/usr/bin/env python3
"""Rebuild the minimal indirect-call analysis consumed by receiver inference.

The receiver-vtable inference only needs source ids plus CALLIND call_index,
call_address, and dispatch-slot candidates. This helper reconstructs exactly that
minimal view from a previous NEVERGONE.StaticReceiverVtableInference/1 output so
later inference stages do not depend on an ephemeral original analysis JSONL.
"""
from __future__ import annotations

import argparse
import json
from collections import defaultdict
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            if row.get("format") != INFERENCE_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {INFERENCE_FORMAT}")
            rows.append(row)
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    seen: set[tuple[str, int]] = set()
    rows = read_jsonl(args.inference_jsonl)
    for line_no, row in enumerate(rows, 1):
        source_id = row.get("source_id")
        call_index = row.get("call_index")
        if not isinstance(source_id, str) or not source_id:
            raise ValueError(f"{args.inference_jsonl}:{line_no}: missing source_id")
        if not isinstance(call_index, int) or call_index < 0:
            raise ValueError(f"{args.inference_jsonl}:{line_no}: invalid call_index")
        key = (source_id, call_index)
        if key in seen:
            raise ValueError(f"duplicate inference key: {source_id}#{call_index}")
        seen.add(key)
        grouped[source_id].append({
            "call_index": call_index,
            "call_address": row.get("call_address"),
            "primary_slot_offset_candidate": row.get("dispatch_slot_offset"),
        })

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for source_id in sorted(grouped):
            calls = sorted(grouped[source_id], key=lambda item: item["call_index"])
            out = {
                "id": source_id,
                "final_action": "review",
                "indirect_call_slice_evidence": {
                    "present": True,
                    "calls": calls,
                    "reconstructed_from_static_receiver_inference": True,
                },
            }
            handle.write(json.dumps(out, sort_keys=True) + "\n")

    print("format: NEVERGONE.ReconstructedReceiverAnalysis/1")
    print("source functions:", len(grouped))
    print("calls:", len(rows))
    print("output:", args.output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
