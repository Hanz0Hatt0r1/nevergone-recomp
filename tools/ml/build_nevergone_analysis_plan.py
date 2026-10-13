#!/usr/bin/env python3
"""Build deterministic local analysis queues from Nevergone function evidence.

The plan is workflow output only. It does not change routing, semantic labels, or
model inputs. Every recommendation already present in ``suggested_analysis_method``
is preserved so multi-signal functions remain visible after their primary step.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import json
import os
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

FORMAT = "NEVERGONE.AnalysisPlan/1"
METHODS = (
    "expand_pcode",
    "resolve_indirect_calls",
    "inspect_callers",
    "inspect_callees",
    "control_flow_review",
    "compare_nearest_family",
    "inspect_memory_dataflow",
    "standard_decompilation_review",
)


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


def read_tsv_gz(path: Path) -> list[dict[str, str]]:
    with gzip.open(path, "rt", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def split_id(record_id: str) -> tuple[str, str]:
    if ":" not in record_id:
        raise ValueError(f"expected program:source_entry id, got {record_id!r}")
    return record_id.split(":", 1)


def sort_entries(entries: list[dict[str, Any]]) -> list[dict[str, Any]]:
    return sorted(entries, key=lambda item: (-int(item["score"]), item["id"]))


def write_jsonl(path: Path, rows: list[dict[str, Any]]) -> None:
    with path.open("w", encoding="utf-8") as handle:
        for row in rows:
            handle.write(json.dumps(row, sort_keys=True) + "\n")


def write_tsv(path: Path, rows: list[dict[str, Any]]) -> None:
    columns = ["id", "source_entry", "score", "primary", "final_action"]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=columns, delimiter="\t")
        writer.writeheader()
        for row in rows:
            writer.writerow({key: row.get(key, "") for key in columns})


def ghidra_runner(entries: list[dict[str, Any]], exporter: str, subdir: str, extension: str) -> str:
    lines = [
        "#!/usr/bin/env bash",
        "set -euo pipefail",
        ': "${GHIDRA_HOME:=/opt/ghidra}"',
        ': "${GHIDRA_PROJECT_DIR:=/home/pes/ghidra_projects/nevergone}"',
        ': "${GHIDRA_PROJECT_NAME:=nevergone}"',
        ': "${GHIDRA_PROGRAM:=libcocos2dcpp.so}"',
        ': "${REPO_ROOT:=$PWD}"',
        f'OUT_ROOT="${{1:-/tmp/nevergone-analysis-evidence}}/{subdir}"',
        'mkdir -p "$OUT_ROOT"',
        'ANALYZE="$GHIDRA_HOME/support/analyzeHeadless"',
        '[[ -x "$ANALYZE" ]] || { echo "missing analyzeHeadless: $ANALYZE" >&2; exit 2; }',
        '[[ -f "$REPO_ROOT/tools/ghidra/' + exporter + '" ]] || { echo "missing exporter under $REPO_ROOT/tools/ghidra" >&2; exit 2; }',
        "",
        "while IFS=$'\\t' read -r entry record_id score; do",
        '  [[ -n "$entry" ]] || continue',
        '  safe="${record_id//[:\\/]/_}"',
        '  echo "[$score] $record_id"',
        '  "$ANALYZE" "$GHIDRA_PROJECT_DIR" "$GHIDRA_PROJECT_NAME" \\\',
        '    -process "$GHIDRA_PROGRAM" -recursive -noanalysis \\\',
        '    -scriptPath "$REPO_ROOT/tools/ghidra" \\\',
        f'    -postScript {exporter} "$entry" "$OUT_ROOT/$safe.{extension}"',
        "done <<'NEVERGONE_ANALYSIS_QUEUE'",
    ]
    for entry in entries:
        lines.append(f"{entry['source_entry']}\t{entry['id']}\t{entry['score']}")
    lines.extend(["NEVERGONE_ANALYSIS_QUEUE", ""])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("analysis", type=Path)
    parser.add_argument("functions", type=Path)
    parser.add_argument("calls", type=Path)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--neighbor-limit", type=int, default=100)
    args = parser.parse_args()
    if args.neighbor_limit < 1:
        raise SystemExit("--neighbor-limit must be positive")

    analysis = read_jsonl(args.analysis)
    if not analysis:
        raise SystemExit("analysis input is empty")

    function_rows = read_tsv_gz(args.functions)
    known_entries = {row.get("entry", "") for row in function_rows if row.get("entry")}
    call_rows = read_tsv_gz(args.calls)

    callers: dict[str, set[str]] = defaultdict(set)
    callees: dict[str, set[str]] = defaultdict(set)
    for edge in call_rows:
        caller = edge.get("caller_entry", "")
        callee = edge.get("callee_entry", "")
        if caller and callee:
            callees[caller].add(callee)
            callers[callee].add(caller)

    primary_counts: Counter[str] = Counter()
    recommendation_counts: Counter[str] = Counter()
    queues: dict[str, list[dict[str, Any]]] = defaultdict(list)

    for row in analysis:
        record_id = row.get("id")
        if not isinstance(record_id, str) or not record_id:
            raise ValueError("analysis row missing id")
        program, source_entry = split_id(record_id)
        suggestion = row.get("suggested_analysis_method")
        if not isinstance(suggestion, dict):
            raise ValueError(f"{record_id}: missing suggested_analysis_method")
        primary = suggestion.get("primary")
        recommendations = suggestion.get("recommendations")
        if not isinstance(primary, str) or primary not in METHODS:
            raise ValueError(f"{record_id}: unsupported primary method {primary!r}")
        if not isinstance(recommendations, list) or not recommendations:
            raise ValueError(f"{record_id}: missing recommendations")
        primary_counts[primary] += 1

        for rec in recommendations:
            if not isinstance(rec, dict):
                raise ValueError(f"{record_id}: malformed recommendation")
            method = rec.get("method")
            score = rec.get("score")
            reasons = rec.get("reasons")
            if method not in METHODS or not isinstance(score, int) or not isinstance(reasons, list):
                raise ValueError(f"{record_id}: malformed recommendation fields")
            recommendation_counts[method] += 1
            entry: dict[str, Any] = {
                "id": record_id,
                "program": program,
                "source_entry": source_entry,
                "source_entry_known": source_entry in known_entries,
                "method": method,
                "score": score,
                "primary": method == primary,
                "reasons": list(reasons),
                "final_action": row.get("final_action"),
            }
            if method == "inspect_callers":
                neighbors = sorted(callers.get(source_entry, set()))
                entry["caller_count"] = len(neighbors)
                entry["callers"] = neighbors[: args.neighbor_limit]
                entry["callers_truncated"] = len(neighbors) > args.neighbor_limit
            elif method == "inspect_callees":
                neighbors = sorted(callees.get(source_entry, set()))
                entry["callee_count"] = len(neighbors)
                entry["callees"] = neighbors[: args.neighbor_limit]
                entry["callees_truncated"] = len(neighbors) > args.neighbor_limit
            queues[method].append(entry)

    out = args.output_dir
    queue_dir = out / "queues"
    command_dir = out / "commands"
    queue_dir.mkdir(parents=True, exist_ok=True)
    command_dir.mkdir(parents=True, exist_ok=True)

    for method in METHODS:
        entries = sort_entries(queues.get(method, []))
        write_jsonl(queue_dir / f"{method}.jsonl", entries)
        write_tsv(queue_dir / f"{method}.tsv", entries)

    expand_entries = sort_entries(queues.get("expand_pcode", []))
    indirect_entries = sort_entries(queues.get("resolve_indirect_calls", []))
    expand_script = command_dir / "run_expand_pcode.sh"
    indirect_script = command_dir / "run_indirect_call_evidence.sh"
    expand_script.write_text(
        ghidra_runner(expand_entries, "ExportFunctionPcodeFlow.java", "expanded-pcode", "tsv"),
        encoding="utf-8",
    )
    indirect_script.write_text(
        ghidra_runner(indirect_entries, "ExportIndirectCallEvidence.java", "indirect-calls", "tsv"),
        encoding="utf-8",
    )
    os.chmod(expand_script, 0o755)
    os.chmod(indirect_script, 0o755)

    manifest = {
        "format": FORMAT,
        "functions": len(analysis),
        "known_function_entries": len(known_entries),
        "resolved_call_edges": len(call_rows),
        "neighbor_limit": args.neighbor_limit,
        "primary_counts": dict(sorted(primary_counts.items())),
        "recommendation_counts": dict(sorted(recommendation_counts.items())),
        "queues": {method: len(queues.get(method, [])) for method in METHODS},
        "routing_modified": False,
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", FORMAT)
    print("functions:", len(analysis))
    print("primary counts:", dict(primary_counts))
    print("all recommendation counts:", dict(recommendation_counts))
    print("output:", out)
    print("routing modified: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
