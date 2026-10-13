#!/usr/bin/env python3
"""Extract objective call-graph metrics for Nevergone functions.

Uses the Ghidra full-index functions/calls TSV exports plus corpus metadata.
This is descriptive evidence only; it does not change routing decisions.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import json
from collections import defaultdict
from pathlib import Path
from typing import Any, Iterable

FORMAT = "NEVERGONE.CallGraphMetrics/1"


def _read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            row = json.loads(raw)
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected JSON object")
            yield row


def _open_tsv(path: Path):
    if path.suffix == ".gz":
        return gzip.open(path, "rt", encoding="utf-8", newline="")
    return path.open("r", encoding="utf-8", newline="")


def _identity(row: dict[str, Any], line_no: int) -> tuple[str, str]:
    record_id = row.get("id")
    program = row.get("program")
    entry = row.get("source_entry")
    if isinstance(record_id, str) and record_id:
        if not isinstance(entry, str) or not entry:
            raise ValueError(f"input row {line_no} has id but no source_entry")
        return record_id, entry
    if isinstance(program, str) and program and isinstance(entry, str) and entry:
        return f"{program}:{entry}", entry
    raise ValueError(f"input row {line_no} has no usable identity")


def _corpus_call_counts(row: dict[str, Any]) -> tuple[int | None, int | None]:
    metrics = row.get("metrics")
    if not isinstance(metrics, dict):
        return None, None
    calls = metrics.get("calls")
    indirect = metrics.get("indirect_calls")
    if not isinstance(calls, int) or calls < 0:
        calls = None
    if not isinstance(indirect, int) or indirect < 0:
        indirect = None
    return calls, indirect


def load_functions(path: Path) -> dict[str, bool]:
    functions: dict[str, bool] = {}
    with _open_tsv(path) as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        required = {"entry", "is_external"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError(f"{path}: expected columns {sorted(required)}")
        for row in reader:
            entry = row["entry"]
            external = row["is_external"].strip().lower() == "true"
            functions[entry] = external
    return functions


def load_calls(path: Path):
    outgoing_sites: dict[str, list[tuple[str, str]]] = defaultdict(list)
    incoming: dict[str, set[str]] = defaultdict(set)
    with _open_tsv(path) as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        required = {"caller_entry", "call_site", "callee_entry"}
        if not required.issubset(reader.fieldnames or []):
            raise ValueError(f"{path}: expected columns {sorted(required)}")
        for row in reader:
            caller = row["caller_entry"]
            call_site = row["call_site"]
            callee = row["callee_entry"]
            outgoing_sites[caller].append((call_site, callee))
            incoming[callee].add(caller)
    return outgoing_sites, incoming


def metrics_for(
    entry: str,
    *,
    functions: dict[str, bool],
    outgoing_sites: dict[str, list[tuple[str, str]]],
    incoming: dict[str, set[str]],
    corpus_calls: int | None,
    corpus_indirect_calls: int | None,
) -> dict[str, Any]:
    sites = outgoing_sites.get(entry, [])
    distinct_callees = {callee for _, callee in sites}
    callers = incoming.get(entry, set())

    internal_callees = {callee for callee in distinct_callees if functions.get(callee) is False}
    external_callees = {callee for callee in distinct_callees if functions.get(callee) is True}
    unknown_callees = distinct_callees - internal_callees - external_callees

    internal_call_sites = sum(1 for _, callee in sites if functions.get(callee) is False)
    external_call_sites = sum(1 for _, callee in sites if functions.get(callee) is True)
    unknown_target_sites = len(sites) - internal_call_sites - external_call_sites

    resolved_sites = len(sites)
    total_calls = corpus_calls
    indirect_calls = corpus_indirect_calls
    if total_calls is None:
        direct_calls = None
        resolved_ratio = None
        unknown_ratio = None
    else:
        indirect_calls = indirect_calls if indirect_calls is not None else 0
        direct_calls = max(0, total_calls - indirect_calls)
        resolved_ratio = min(1.0, resolved_sites / total_calls) if total_calls else 1.0
        unknown_ratio = max(0.0, 1.0 - resolved_ratio)

    return {
        "fan_in": len(callers),
        "fan_out": len(distinct_callees),
        "resolved_call_site_count": resolved_sites,
        "distinct_callee_count": len(distinct_callees),
        "distinct_caller_count": len(callers),
        "internal_call_site_count": internal_call_sites,
        "external_call_site_count": external_call_sites,
        "unknown_target_call_site_count": unknown_target_sites,
        "internal_callee_count": len(internal_callees),
        "external_callee_count": len(external_callees),
        "unknown_callee_count": len(unknown_callees),
        "corpus_call_count": total_calls,
        "corpus_indirect_call_count": indirect_calls,
        "corpus_direct_call_count": direct_calls,
        "resolved_call_ratio_proxy": resolved_ratio,
        "unknown_call_ratio_proxy": unknown_ratio,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corpus", type=Path)
    parser.add_argument("functions_tsv", type=Path)
    parser.add_argument("calls_tsv", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    functions = load_functions(args.functions_tsv)
    outgoing_sites, incoming = load_calls(args.calls_tsv)
    rows = list(_read_jsonl(args.corpus))
    if not rows:
        raise SystemExit("input is empty")

    seen: set[str] = set()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", encoding="utf-8") as handle:
        for line_no, row in enumerate(rows, 1):
            record_id, entry = _identity(row, line_no)
            if record_id in seen:
                raise ValueError(f"duplicate id: {record_id}")
            calls, indirect = _corpus_call_counts(row)
            output = {
                "format": FORMAT,
                "id": record_id,
                "source_entry": entry,
                "metrics": metrics_for(
                    entry,
                    functions=functions,
                    outgoing_sites=outgoing_sites,
                    incoming=incoming,
                    corpus_calls=calls,
                    corpus_indirect_calls=indirect,
                ),
            }
            handle.write(json.dumps(output, sort_keys=True) + "\n")
            seen.add(record_id)

    print("format:", FORMAT)
    print("functions:", len(seen))
    print("known function entries:", len(functions))
    print("resolved call edges:", sum(len(v) for v in outgoing_sites.values()))
    print("output:", args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
