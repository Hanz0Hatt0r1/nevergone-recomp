#!/usr/bin/env python3
from __future__ import annotations

import argparse, csv, json
from pathlib import Path

INF_FMT = "NEVERGONE.StaticReceiverVtableInference/1"
STRUCT_FMT = "NEVERGONE.GhidraStructuralReceiverParameterProvenance/1"
TARGET_STATUS = "caller_calls_no_single_pointer_type"


def read_jsonl(path: Path):
    with path.open(encoding="utf-8") as f:
        return [json.loads(x) for x in f if x.strip()]


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("inference_jsonl", type=Path)
    p.add_argument("structural_tsv", type=Path)
    p.add_argument("output_tsv", type=Path)
    a = p.parse_args()

    unresolved = {}
    for i, row in enumerate(read_jsonl(a.inference_jsonl), 1):
        if row.get("format") != INF_FMT:
            raise ValueError(f"{a.inference_jsonl}:{i}: expected {INF_FMT}")
        if row.get("status") == "receiver_not_single_pointer":
            unresolved[(row.get("source_id"), row.get("call_index"))] = row

    planned = []
    with a.structural_tsv.open(encoding="utf-8", newline="") as f:
        for line_no, row in enumerate(csv.DictReader(f, delimiter="\t"), 2):
            if row.get("format") != STRUCT_FMT:
                raise ValueError(f"{a.structural_tsv}:{line_no}: expected {STRUCT_FMT}")
            try:
                call_index = int(row.get("call_index", "-1"), 10)
            except ValueError:
                continue
            key = (row.get("source_id"), call_index)
            inf = unresolved.get(key)
            if inf is None or row.get("status") != TARGET_STATUS:
                continue
            param = row.get("parameter_index")
            if param in (None, "", "-"):
                continue
            planned.append({
                "source_id": key[0],
                "source_entry": row.get("function_entry") or key[0].split(":", 1)[-1],
                "source_call_index": call_index,
                "parameter_index": int(param, 10),
                "dispatch_slot_offset": inf.get("dispatch_slot_offset") or "-",
                "expected_direct_caller_functions": int(row.get("direct_caller_count") or 0),
                "expected_matched_caller_calls": int(row.get("matched_caller_calls") or 0),
            })

    planned.sort(key=lambda r: (r["source_id"], r["source_call_index"]))
    a.output_tsv.parent.mkdir(parents=True, exist_ok=True)
    fields = ["source_id", "source_entry", "source_call_index", "parameter_index", "dispatch_slot_offset",
              "expected_direct_caller_functions", "expected_matched_caller_calls"]
    with a.output_tsv.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, delimiter="\t")
        w.writeheader(); w.writerows(planned)

    print("format: NEVERGONE.CallerArgumentSubtreePlan/1")
    print("planned rows:", len(planned))
    for r in planned:
        print(r["source_id"], "call", r["source_call_index"], "param=", r["parameter_index"],
              "callers=", r["expected_direct_caller_functions"], "matched=", r["expected_matched_caller_calls"])
    print("output:", a.output_tsv)
    print("affects routing: no")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
