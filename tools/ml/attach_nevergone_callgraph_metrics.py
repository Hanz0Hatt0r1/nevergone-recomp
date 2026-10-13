#!/usr/bin/env python3
"""Attach call-graph metrics to Nevergone analysis without changing routing."""
from __future__ import annotations

import argparse
import json
from pathlib import Path


def read_jsonl(path: Path) -> list[dict]:
    rows: list[dict] = []
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
    parser.add_argument("analysis", type=Path)
    parser.add_argument("metrics", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    metric_rows = read_jsonl(args.metrics)
    by_id: dict[str, dict] = {}
    for row in metric_rows:
        record_id = row.get("id")
        metrics = row.get("metrics")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("metric row missing id")
        if not isinstance(metrics, dict):
            raise ValueError(f"metric row {record_id} missing metrics")
        if record_id in by_id:
            raise ValueError(f"duplicate metric id: {record_id}")
        by_id[record_id] = metrics

    rows = read_jsonl(args.analysis)
    before_actions = [row.get("final_action") for row in rows]
    missing: list[str] = []
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for row in rows:
            record_id = row.get("id")
            if not isinstance(record_id, str) or not record_id:
                raise ValueError("analysis row missing id")
            metrics = by_id.get(record_id)
            if metrics is None:
                missing.append(record_id)
                continue
            out = dict(row)
            out["callgraph_metrics"] = metrics
            handle.write(json.dumps(out, sort_keys=True) + "\n")

    if missing:
        args.output.unlink(missing_ok=True)
        raise ValueError(f"missing callgraph metrics for {len(missing)} analysis rows")

    after = read_jsonl(args.output)
    after_actions = [row.get("final_action") for row in after]
    if before_actions != after_actions:
        args.output.unlink(missing_ok=True)
        raise AssertionError("attaching callgraph metrics changed final_action")

    print("functions:", len(rows))
    print("attached:", len(rows))
    print("final_action unchanged: yes")
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
