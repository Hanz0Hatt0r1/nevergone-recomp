#!/usr/bin/env python3
"""Join ML classification output with Ghidra index metadata for external audit.

This tool is deliberately outside the model pipeline. Function names, signatures
and call-neighbour names are used only after inference to inspect predictions;
they are never fed back into the production classifier.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any, Iterable

DEFAULT_INDEX_ROOT = Path("docs/ghidra/full-index")


def read_jsonl(path: Path) -> Iterable[dict[str, Any]]:
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            try:
                row = json.loads(raw)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{line_no}: invalid JSON: {exc}") from exc
            if not isinstance(row, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            yield row


def open_tsv(path: Path) -> Iterable[dict[str, str]]:
    with gzip.open(path, "rt", encoding="utf-8", newline="") as handle:
        yield from csv.DictReader(handle, delimiter="\t")


def normalize_entry(value: str) -> str:
    value = value.strip().lower()
    if value.startswith("0x"):
        value = value[2:]
    if not value:
        raise ValueError("empty entry")
    return f"{int(value, 16):08x}"


def parse_record_id(record_id: str) -> tuple[str, str]:
    if ":" not in record_id:
        raise ValueError(f"classification id has no program/address separator: {record_id!r}")
    program, entry = record_id.rsplit(":", 1)
    program = Path(program).name
    if program.endswith(".so"):
        program = program[:-3]
    return program, normalize_entry(entry)


def namespace_hint(name: str) -> str:
    if "::" in name:
        return name.split("::", 1)[0]
    stripped = name.lstrip("_")
    if not stripped:
        return "<unnamed>"
    if "_" in stripped:
        return stripped.split("_", 1)[0]
    return stripped


def load_functions(index_root: Path, program: str) -> dict[str, dict[str, str]]:
    path = index_root / f"{program}.functions.tsv.gz"
    if not path.is_file():
        raise FileNotFoundError(f"missing function index: {path}")
    result: dict[str, dict[str, str]] = {}
    for row in open_tsv(path):
        entry = row.get("entry", "")
        if not entry:
            continue
        result[normalize_entry(entry)] = row
    return result


def collect_call_context(
    index_root: Path,
    program: str,
    selected_entries: set[str],
    function_names: dict[str, str],
    sample_limit: int,
) -> dict[str, dict[str, Any]]:
    context = {
        entry: {
            "caller_count": 0,
            "callee_count": 0,
            "callers": [],
            "callees": [],
        }
        for entry in selected_entries
    }
    path = index_root / f"{program}.calls.tsv.gz"
    if not path.is_file():
        return context

    for row in open_tsv(path):
        caller_raw = row.get("caller_entry", "")
        callee_raw = row.get("callee_entry", "")
        try:
            caller = normalize_entry(caller_raw) if caller_raw else ""
            callee = normalize_entry(callee_raw) if callee_raw else ""
        except ValueError:
            continue

        if caller in context:
            item = context[caller]
            item["callee_count"] += 1
            if len(item["callees"]) < sample_limit:
                item["callees"].append(function_names.get(callee, callee or "<unknown>"))
        if callee in context:
            item = context[callee]
            item["caller_count"] += 1
            if len(item["callers"]) < sample_limit:
                item["callers"].append(function_names.get(caller, caller or "<unknown>"))
    return context


def assign_address_clusters(rows: list[dict[str, Any]], max_gap: int) -> None:
    by_program: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for row in rows:
        by_program[row["program"]].append(row)

    next_cluster = 1
    for program in sorted(by_program):
        ordered = sorted(by_program[program], key=lambda row: int(row["entry"], 16))
        previous: int | None = None
        cluster = next_cluster
        for row in ordered:
            address = int(row["entry"], 16)
            if previous is not None and address - previous > max_gap:
                cluster += 1
            row["cluster_id"] = f"{program}:{cluster}"
            previous = address
        next_cluster = cluster + 1


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("classified", type=Path, help="classifier JSONL output")
    parser.add_argument("--index-root", type=Path, default=DEFAULT_INDEX_ROOT)
    parser.add_argument("--action", action="append", choices=("accept", "review", "escalate"))
    parser.add_argument("--prediction", action="append", help="keep only one or more predicted classes")
    parser.add_argument("--output", type=Path, help="write TSV here; default stdout")
    parser.add_argument("--call-samples", type=int, default=4)
    parser.add_argument("--cluster-gap", type=lambda x: int(x, 0), default=0x100)
    args = parser.parse_args()

    allowed_actions = set(args.action or ["accept"])
    allowed_predictions = set(args.prediction or [])

    selected: list[dict[str, Any]] = []
    for row in read_jsonl(args.classified):
        if row.get("action") not in allowed_actions:
            continue
        if allowed_predictions and row.get("prediction") not in allowed_predictions:
            continue
        record_id = row.get("id")
        if not isinstance(record_id, str):
            continue
        try:
            program, entry = parse_record_id(record_id)
        except ValueError:
            continue
        selected.append({"classification": row, "program": program, "entry": entry})

    if not selected:
        print("no matching classification rows", file=sys.stderr)
        return 1

    programs = sorted({row["program"] for row in selected})
    function_tables: dict[str, dict[str, dict[str, str]]] = {}
    call_context: dict[str, dict[str, dict[str, Any]]] = {}

    for program in programs:
        functions = load_functions(args.index_root, program)
        function_tables[program] = functions
        names = {entry: row.get("name", "") for entry, row in functions.items()}
        entries = {row["entry"] for row in selected if row["program"] == program}
        call_context[program] = collect_call_context(
            args.index_root,
            program,
            entries,
            names,
            args.call_samples,
        )

    audit_rows: list[dict[str, Any]] = []
    missing = 0
    for item in selected:
        cls = item["classification"]
        program = item["program"]
        entry = item["entry"]
        function = function_tables[program].get(entry)
        if function is None:
            missing += 1
            function = {}
        context = call_context[program].get(entry, {})
        name = function.get("name", "<missing>")
        audit_rows.append(
            {
                "program": program,
                "entry": entry,
                "cluster_id": "",
                "prediction": cls.get("prediction", ""),
                "action": cls.get("action", ""),
                "confidence": cls.get("confidence", ""),
                "family_similarity": cls.get("family_similarity", ""),
                "nearest_training_family": cls.get("nearest_training_family", ""),
                "nearest_competing_label": cls.get("nearest_competing_label", ""),
                "competing_similarity": cls.get("competing_similarity", ""),
                "contrast_margin": cls.get("contrast_margin", ""),
                "name": name,
                "namespace_hint": namespace_hint(name),
                "signature": function.get("signature", ""),
                "address_count": function.get("address_count", ""),
                "caller_count": context.get("caller_count", 0),
                "callee_count": context.get("callee_count", 0),
                "callers_sample": " | ".join(context.get("callers", [])),
                "callees_sample": " | ".join(context.get("callees", [])),
            }
        )

    assign_address_clusters(audit_rows, args.cluster_gap)
    audit_rows.sort(key=lambda row: (row["program"], int(row["entry"], 16)))

    fields = [
        "program", "entry", "cluster_id", "prediction", "action", "confidence",
        "family_similarity", "nearest_training_family", "nearest_competing_label",
        "competing_similarity", "contrast_margin", "name", "namespace_hint",
        "signature", "address_count", "caller_count", "callee_count",
        "callers_sample", "callees_sample",
    ]

    handle = args.output.open("w", encoding="utf-8", newline="") if args.output else sys.stdout
    try:
        writer = csv.DictWriter(handle, fieldnames=fields, delimiter="\t", lineterminator="\n")
        writer.writeheader()
        writer.writerows(audit_rows)
    finally:
        if args.output:
            handle.close()

    print(f"selected: {len(audit_rows)}", file=sys.stderr)
    print(f"missing index functions: {missing}", file=sys.stderr)
    print("predictions:", Counter(row["prediction"] for row in audit_rows), file=sys.stderr)
    print("namespaces:", Counter(row["namespace_hint"] for row in audit_rows).most_common(20), file=sys.stderr)
    print("clusters:", Counter(row["cluster_id"] for row in audit_rows).most_common(), file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
