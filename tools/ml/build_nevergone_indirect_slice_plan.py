#!/usr/bin/env python3
"""Build a bounded CALLIND provenance-slice queue from pattern evidence."""
from __future__ import annotations

import argparse
import csv
import json
import os
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.IndirectSlicePlan/1"


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


def source_entry(record_id: str) -> str:
    if ":" not in record_id:
        raise ValueError(f"expected program:source_entry id, got {record_id!r}")
    return record_id.split(":", 1)[1]


def selected_row(row: dict[str, Any], include_all: bool) -> bool:
    evidence = row.get("indirect_call_pattern_evidence")
    if not isinstance(evidence, dict) or not evidence.get("present"):
        return False
    if include_all:
        return int(evidence.get("call_count", 0) or 0) > 0
    return int(evidence.get("depth_limited_expression_count", 0) or 0) > 0


def runner(entries: list[dict[str, Any]], max_nodes: int, max_depth: int) -> str:
    lines = [
        "#!/usr/bin/env bash",
        "set -euo pipefail",
        ': "${GHIDRA_HOME:=/opt/ghidra}"',
        ': "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"',
        ': "${GHIDRA_PROJECT_NAME:=nevergone}"',
        ': "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"',
        ': "${REPO_ROOT:=$PWD}"',
        f': "${{MAX_NODES:={max_nodes}}}"',
        f': "${{MAX_DEPTH:={max_depth}}}"',
        'OUT_ROOT="${1:-/tmp/nevergone-analysis-evidence}/indirect-slices"',
        'mkdir -p "$OUT_ROOT"',
        'ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"',
        'EXPORTER="$REPO_ROOT/tools/ghidra/ExportIndirectCallSlice.java"',
        '[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }',
        '[[ -f "$EXPORTER" ]] || { echo "missing exporter: $EXPORTER" >&2; exit 2; }',
        "",
        "while IFS=$'\\t' read -r entry record_id calls limited; do",
        '  [[ -n "$entry" ]] || continue',
        '  safe="${record_id//[:\\/]/_}"',
        '  echo "[$limited/$calls depth-limited] $record_id"',
        '  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" -process "$GHIDRA_PROGRAM" -recursive -noanalysis -scriptPath "$REPO_ROOT/tools/ghidra" -postScript ExportIndirectCallSlice.java "$entry" "$OUT_ROOT/$safe.tsv" "$MAX_NODES" "$MAX_DEPTH"',
        "done <<'NEVERGONE_INDIRECT_SLICE_QUEUE'",
    ]
    for item in entries:
        lines.append(
            f"{item['source_entry']}\t{item['id']}\t{item['call_count']}\t{item['depth_limited_expression_count']}"
        )
    lines.extend(["NEVERGONE_INDIRECT_SLICE_QUEUE", ""])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--max-nodes", type=int, default=96)
    parser.add_argument("--max-depth", type=int, default=24)
    parser.add_argument("--include-all-indirect", action="store_true")
    args = parser.parse_args()
    if args.max_nodes < 8:
        raise SystemExit("--max-nodes must be >= 8")
    if args.max_depth < 2:
        raise SystemExit("--max-depth must be >= 2")

    rows = read_jsonl(args.analysis)
    entries: list[dict[str, Any]] = []
    for row in rows:
        if not selected_row(row, args.include_all_indirect):
            continue
        record_id = row.get("id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("analysis row missing id")
        evidence = row["indirect_call_pattern_evidence"]
        entries.append(
            {
                "id": record_id,
                "source_entry": source_entry(record_id),
                "final_action": row.get("final_action"),
                "call_count": int(evidence.get("call_count", 0) or 0),
                "depth_limited_expression_count": int(
                    evidence.get("depth_limited_expression_count", 0) or 0
                ),
                "depth_limited_fraction": float(evidence.get("depth_limited_fraction", 0.0) or 0.0),
                "previous_max_depth": evidence.get("nesting_depth_max"),
                "max_nodes": args.max_nodes,
                "max_depth": args.max_depth,
            }
        )

    entries.sort(
        key=lambda item: (
            -item["depth_limited_expression_count"],
            -item["call_count"],
            item["id"],
        )
    )

    out = args.output_dir
    out.mkdir(parents=True, exist_ok=True)
    queue_jsonl = out / "queue.jsonl"
    with queue_jsonl.open("w", encoding="utf-8") as handle:
        for item in entries:
            handle.write(json.dumps(item, sort_keys=True) + "\n")

    queue_tsv = out / "queue.tsv"
    columns = [
        "id",
        "source_entry",
        "final_action",
        "call_count",
        "depth_limited_expression_count",
        "depth_limited_fraction",
        "previous_max_depth",
        "max_nodes",
        "max_depth",
    ]
    with queue_tsv.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, delimiter="\t")
        writer.writeheader()
        writer.writerows(entries)

    command = out / "run_indirect_call_slices.sh"
    command.write_text(runner(entries, args.max_nodes, args.max_depth), encoding="utf-8")
    os.chmod(command, 0o755)

    manifest = {
        "format": FORMAT,
        "functions": len(rows),
        "selected_functions": len(entries),
        "selected_calls": sum(item["call_count"] for item in entries),
        "selected_depth_limited_calls": sum(
            item["depth_limited_expression_count"] for item in entries
        ),
        "include_all_indirect": args.include_all_indirect,
        "max_nodes": args.max_nodes,
        "max_depth": args.max_depth,
        "routing_modified": False,
    }
    (out / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    print("format:", FORMAT)
    print("functions:", len(rows))
    print("selected functions:", len(entries))
    print("selected calls:", manifest["selected_calls"])
    print("selected depth-limited calls:", manifest["selected_depth_limited_calls"])
    print("max nodes:", args.max_nodes)
    print("max depth:", args.max_depth)
    print("routing modified: no")
    print("output:", out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
