#!/usr/bin/env python3
"""Validate and summarize a local Nevergone MlPcodeCorpus/2 JSONL export."""
from __future__ import annotations

import argparse
import json
from collections import Counter
from pathlib import Path
from typing import Any

import build_nevergone_dataset as dataset


def percentile(values: list[int], fraction: float) -> int:
    if not values:
        return 0
    ordered = sorted(values)
    index = round((len(ordered) - 1) * fraction)
    return ordered[index]


def header_value(text: str, key: str) -> str:
    prefix = key + "="
    for line in text.splitlines():
        if line.startswith(prefix):
            return line[len(prefix):]
    return "unknown"


def summarize(path: Path) -> dict[str, Any]:
    stats: Counter = Counter()
    architectures: Counter = Counter()
    callconvs: Counter = Counter()
    op_counts: list[int] = []
    emitted_op_counts: list[int] = []
    model_bytes: list[int] = []

    for row in dataset.read_jsonl(path):
        stats["rows"] += 1
        if row.get("format") != dataset.CORPUS_FORMAT:
            raise ValueError(
                f"unsupported corpus format {row.get('format')!r}; "
                f"expected {dataset.CORPUS_FORMAT}"
            )
        status = row.get("decompile_status")
        if status != "ok":
            stats["failed"] += 1
            continue

        text = row.get("model_text")
        if not isinstance(text, str) or not text:
            raise ValueError("successful corpus row has no model_text")
        dataset.validate_model_text(row, text)
        stats["ok"] += 1
        architectures[header_value(text, "ARCH")] += 1
        callconvs[header_value(text, "CALLCONV")] += 1
        model_bytes.append(len(text.encode("utf-8")))

        metrics = row.get("metrics")
        if not isinstance(metrics, dict):
            raise ValueError("successful MlPcodeCorpus/2 row has no metrics object")
        ops = metrics.get("ops")
        emitted = metrics.get("emitted_ops")
        if not isinstance(ops, int) or ops < 0:
            raise ValueError("metrics.ops must be a non-negative integer")
        if not isinstance(emitted, int) or emitted < 0:
            raise ValueError("metrics.emitted_ops must be a non-negative integer")
        if emitted > dataset.MAX_MODEL_OP_LINES:
            raise ValueError(
                f"emitted_ops={emitted} exceeds cap {dataset.MAX_MODEL_OP_LINES}"
            )
        op_counts.append(ops)
        emitted_op_counts.append(emitted)
        if metrics.get("truncated") is True:
            stats["truncated"] += 1

    return {
        "format": dataset.CORPUS_FORMAT,
        "path": str(path),
        "rows": stats["rows"],
        "ok": stats["ok"],
        "failed": stats["failed"],
        "truncated": stats["truncated"],
        "architectures": dict(architectures),
        "calling_conventions": dict(callconvs),
        "ops": {
            "median": percentile(op_counts, 0.50),
            "p95": percentile(op_counts, 0.95),
            "max": max(op_counts, default=0),
        },
        "emitted_ops": {
            "median": percentile(emitted_op_counts, 0.50),
            "p95": percentile(emitted_op_counts, 0.95),
            "max": max(emitted_op_counts, default=0),
        },
        "model_text_bytes": {
            "median": percentile(model_bytes, 0.50),
            "p95": percentile(model_bytes, 0.95),
            "max": max(model_bytes, default=0),
        },
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    args = parser.parse_args()

    report = summarize(args.corpus)
    if args.json:
        print(json.dumps(report, indent=2, sort_keys=True))
        return 0

    print(f"format: {report['format']}")
    print(f"rows: {report['rows']} ok={report['ok']} failed={report['failed']}")
    print(f"truncated: {report['truncated']}")
    print(f"architectures: {report['architectures']}")
    print(f"calling conventions: {report['calling_conventions']}")
    print(
        "ops: median={median} p95={p95} max={max}".format(**report["ops"])
    )
    print(
        "emitted ops: median={median} p95={p95} max={max}".format(
            **report["emitted_ops"]
        )
    )
    print(
        "model bytes: median={median} p95={p95} max={max}".format(
            **report["model_text_bytes"]
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
