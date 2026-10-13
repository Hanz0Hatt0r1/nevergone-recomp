#!/usr/bin/env python3
"""Build a bounded Ghidra export plan for CALLIND receiver type evidence."""
from __future__ import annotations

import argparse
import csv
import json
import os
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.ReceiverTypePlan/1"


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


def source_entry(record_id: str) -> str:
    if ":" not in record_id:
        raise ValueError(f"expected program:entry id, got {record_id!r}")
    return record_id.split(":", 1)[1]


def selected(row: dict[str, Any]) -> tuple[bool, int]:
    evidence = row.get("indirect_call_slice_evidence")
    if not isinstance(evidence, dict) or not evidence.get("present"):
        return False, 0
    calls = evidence.get("calls")
    if not isinstance(calls, list) or not calls:
        return False, 0
    return True, len(calls)


def runner() -> str:
    return """#!/usr/bin/env bash
set -euo pipefail
: "${GHIDRA_HOME:=/opt/ghidra}"
: "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"
: "${GHIDRA_PROJECT_NAME:=nevergone}"
: "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"
: "${REPO_ROOT:=$PWD}"
PLAN_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLAN="$PLAN_DIR/plan.tsv"
OUT="${1:-/tmp/nevergone-indirect-receiver-types.tsv}"
ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"
[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }
[[ -f "$REPO_ROOT/tools/ghidra/ExportIndirectCallReceiverTypes.java" ]] || { echo "missing receiver exporter" >&2; exit 2; }
"$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \
  -process "$GHIDRA_PROGRAM" -recursive -readOnly -noanalysis \
  -scriptPath "$REPO_ROOT/tools/ghidra" \
  -postScript ExportIndirectCallReceiverTypes.java "$PLAN" "$OUT"
"""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    rows = read_jsonl(args.analysis)
    entries: list[dict[str, Any]] = []
    for row in rows:
        ok, calls = selected(row)
        if not ok:
            continue
        record_id = row.get("id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("analysis row missing id")
        entries.append({
            "source_id": record_id,
            "source_entry": source_entry(record_id),
            "expected_calls": calls,
        })

    entries.sort(key=lambda x: x["source_id"])
    out = args.output_dir
    out.mkdir(parents=True, exist_ok=True)

    with (out / "plan.tsv").open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=["source_id", "source_entry", "expected_calls"],
            delimiter="\t",
        )
        writer.writeheader()
        writer.writerows(entries)

    script = out / "run_receiver_type_export.sh"
    script.write_text(runner(), encoding="utf-8")
    os.chmod(script, 0o755)

    manifest = {
        "format": FORMAT,
        "analysis_functions": len(rows),
        "selected_functions": len(entries),
        "selected_calls": sum(x["expected_calls"] for x in entries),
        "routing_modified": False,
    }
    (out / "manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )

    print("format:", FORMAT)
    print("analysis functions:", len(rows))
    print("selected functions:", manifest["selected_functions"])
    print("selected calls:", manifest["selected_calls"])
    print("routing modified: no")
    print("output:", out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
