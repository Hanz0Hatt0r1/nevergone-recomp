#!/usr/bin/env python3
"""Remove non-semantic function origins from an existing Nevergone dataset.

The semantic code-domain model is intended for internal implementation
functions. Imported symbols and linker init/fini scaffolding are objective
function-origin categories and are routed before ML instead of being learned as
`game`/`engine`/`runtime_library`/`media_library` examples.
"""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

from nevergone_function_origin import classify_function_origin


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows = []
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
    parser.add_argument("input_dataset", type=Path)
    parser.add_argument("output_dataset", type=Path)
    args = parser.parse_args()

    trace_path = args.input_dataset / "trace.jsonl"
    trace_rows = read_jsonl(trace_path)
    origin_by_id: dict[str, str] = {}
    trace_by_id: dict[str, dict[str, Any]] = {}
    origin_counts: Counter[str] = Counter()
    for row in trace_rows:
        record_id = row.get("id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("trace row missing id")
        origin = classify_function_origin(row.get("source_name"))
        origin_by_id[record_id] = origin
        trace_by_id[record_id] = row
        origin_counts[origin] += 1

    args.output_dataset.mkdir(parents=True, exist_ok=True)
    kept_ids: set[str] = set()
    split_counts: Counter[str] = Counter()
    removed_by_split: Counter[str] = Counter()

    for split in ("train", "validation", "test"):
        rows = read_jsonl(args.input_dataset / f"{split}.jsonl")
        out_path = args.output_dataset / f"{split}.jsonl"
        with out_path.open("w", encoding="utf-8") as out:
            for row in rows:
                record_id = row.get("id")
                origin = origin_by_id.get(record_id)
                if origin is None:
                    raise ValueError(f"dataset id missing from trace: {record_id}")
                if origin != "internal":
                    removed_by_split[split] += 1
                    continue
                out.write(json.dumps(row, sort_keys=True) + "\n")
                kept_ids.add(record_id)
                split_counts[split] += 1

    with (args.output_dataset / "trace.jsonl").open("w", encoding="utf-8") as out:
        for row in trace_rows:
            if row.get("id") not in kept_ids:
                continue
            enriched = dict(row)
            enriched["function_origin"] = "internal"
            out.write(json.dumps(enriched, sort_keys=True) + "\n")

    source_manifest_path = args.input_dataset / "manifest.json"
    source_manifest = {}
    if source_manifest_path.is_file():
        source_manifest = json.loads(source_manifest_path.read_text(encoding="utf-8"))

    report = {
        "format": "NEVERGONE.OriginFilteredDataset/1",
        "source_dataset": str(args.input_dataset),
        "source_format": source_manifest.get("format"),
        "input_trace_rows": len(trace_rows),
        "origin_distribution": dict(sorted(origin_counts.items())),
        "removed_functions": sum(count for origin, count in origin_counts.items() if origin != "internal"),
        "kept_functions": len(kept_ids),
        "splits": dict(sorted(split_counts.items())),
        "removed_by_split": dict(sorted(removed_by_split.items())),
        "policy": {
            "semantic_model_origins": ["internal"],
            "external_import_routed_before_ml": True,
            "runtime_scaffold_routed_before_ml": True,
            "source_name_used_as_model_feature": False,
        },
    }
    (args.output_dataset / "manifest.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    print("format:", report["format"])
    print("input functions:", len(trace_rows))
    print("origins:", origin_counts)
    print("removed:", report["removed_functions"])
    print("kept:", report["kept_functions"])
    print("splits:", report["splits"])
    print("output:", args.output_dataset)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
