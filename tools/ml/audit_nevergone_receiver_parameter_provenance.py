#!/usr/bin/env python3
"""Audit whether function-parameter and direct-caller type provenance recovers receiver classes.

This diagnostic joins unresolved StaticReceiverVtableInference rows with evidence from
ExportIndirectCallReceiverParameterProvenance.java and the static vtable catalog. It
only reports conservative candidate recovery and never changes inference or routing.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
PROVENANCE_FORMAT = "NEVERGONE.GhidraReceiverParameterProvenance/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
RESOLVED = {"resolved_unique_vtable", "resolved_unique_vtable_runtime_match"}


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


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def clean_type(value: Any) -> str | None:
    if not isinstance(value, str):
        return None
    text = value.strip()
    if not text or text == "-":
        return None
    text = re.sub(r"^(?:class|struct)\s+", "", text)
    text = re.sub(r"\s+", " ", text).strip()
    return text or None


def terminal(name: str) -> str:
    return name.rsplit("::", 1)[-1]


def parse_json_strings(value: Any) -> list[str]:
    if value in (None, "", "-"):
        return []
    try:
        parsed = json.loads(str(value))
    except json.JSONDecodeError:
        return []
    if not isinstance(parsed, list):
        return []
    return [x for x in parsed if isinstance(x, str) and x]


def parse_int(value: Any, default: int = 0) -> int:
    if value in (None, "", "-"):
        return default
    try:
        return int(str(value), 0)
    except ValueError:
        return default


def map_candidates(
    raw_candidates: list[str],
    catalog_classes: set[str],
    terminal_to_classes: dict[str, set[str]],
) -> set[str]:
    mapped: set[str] = set()
    for raw in raw_candidates:
        cleaned = clean_type(raw)
        if cleaned is None:
            continue
        if cleaned in catalog_classes:
            mapped.add(cleaned)
            continue
        matches = terminal_to_classes.get(terminal(cleaned), set())
        if len(matches) == 1:
            mapped.update(matches)
    return mapped


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("parameter_provenance_tsv", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    parser.add_argument("--samples", type=int, default=60)
    args = parser.parse_args()

    inference_rows = read_jsonl(args.inference_jsonl)
    provenance_rows = read_tsv(args.parameter_provenance_tsv)
    catalog_rows = read_jsonl(args.catalog_jsonl)

    catalog_classes: set[str] = set()
    for line_no, row in enumerate(catalog_rows, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{args.catalog_jsonl}:{line_no}: expected {CATALOG_FORMAT}")
        name = row.get("class_name")
        if isinstance(name, str) and name:
            catalog_classes.add(name)

    terminal_to_classes: dict[str, set[str]] = defaultdict(set)
    for class_name in catalog_classes:
        terminal_to_classes[terminal(class_name)].add(class_name)

    provenance_by_key: dict[tuple[str, int], dict[str, str]] = {}
    formats = Counter()
    for line_no, row in enumerate(provenance_rows, 2):
        fmt = row.get("format") or "<missing>"
        formats[fmt] += 1
        if fmt != PROVENANCE_FORMAT:
            raise ValueError(f"{args.parameter_provenance_tsv}:{line_no}: expected {PROVENANCE_FORMAT}")
        source_id = row.get("source_id")
        try:
            call_index = int(row.get("call_index", "-1"))
        except ValueError:
            continue
        if source_id and call_index >= 0:
            provenance_by_key[(source_id, call_index)] = row

    joined: list[dict[str, Any]] = []
    original_statuses = Counter()
    provenance_statuses = Counter()
    recovery_statuses = Counter()
    parameter_indexes = Counter()
    recovered_classes = Counter()
    root_parameter_rows = 0
    rows_with_direct_callers = 0
    rows_with_matched_calls = 0
    rows_with_type_candidates = 0

    for inf in inference_rows:
        if inf.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"expected {INFERENCE_FORMAT}")
        original_status = inf.get("status")
        if original_status in RESOLVED:
            continue
        source_id = inf.get("source_id")
        call_index = inf.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            continue
        original_statuses[str(original_status)] += 1

        prov = provenance_by_key.get((source_id, call_index))
        if prov is None:
            provenance_status = "<missing>"
            recovery_status = "missing_parameter_provenance"
            raw_candidates: list[str] = []
            mapped: set[str] = set()
            param_index = -1
            direct_callers = 0
            matched_calls = 0
        else:
            provenance_status = prov.get("status") or "<missing>"
            provenance_statuses[provenance_status] += 1
            param_index = parse_int(prov.get("parameter_index"), -1)
            direct_callers = parse_int(prov.get("direct_caller_count"))
            matched_calls = parse_int(prov.get("matched_caller_calls"))
            raw_candidates = []
            raw_candidates.extend(parse_json_strings(prov.get("caller_single_pointer_pointee_displays_json")))
            raw_candidates.extend(parse_json_strings(prov.get("caller_single_pointer_pointee_names_json")))
            mapped = map_candidates(raw_candidates, catalog_classes, terminal_to_classes)

            if param_index >= 0:
                root_parameter_rows += 1
                parameter_indexes[param_index] += 1
            if direct_callers > 0:
                rows_with_direct_callers += 1
            if matched_calls > 0:
                rows_with_matched_calls += 1
            if raw_candidates:
                rows_with_type_candidates += 1

            if provenance_status == "not_load":
                recovery_status = "receiver_not_load"
            elif param_index < 0:
                recovery_status = "root_not_parameter"
            elif matched_calls == 0:
                recovery_status = "parameter_no_direct_caller_calls"
            elif not raw_candidates:
                recovery_status = "caller_calls_no_type_candidate"
            elif not mapped:
                recovery_status = "caller_candidate_not_in_catalog"
            elif len(mapped) > 1:
                recovery_status = "ambiguous_catalog_class"
            else:
                recovery_status = "unique_catalog_class"
                recovered_classes[next(iter(mapped))] += 1

        recovery_statuses[recovery_status] += 1
        joined.append({
            "source_id": source_id,
            "call_index": call_index,
            "call_address": inf.get("call_address"),
            "original_status": original_status,
            "dispatch_slot_offset": inf.get("dispatch_slot_offset"),
            "provenance_status": provenance_status,
            "recovery_status": recovery_status,
            "root_token": prov.get("root_token") if prov else None,
            "root_type_display": prov.get("root_type_display") if prov else None,
            "root_trace_patterns": parse_json_strings(prov.get("root_trace_patterns_json")) if prov else [],
            "root_trace_nodes": parse_int(prov.get("root_trace_nodes")) if prov else 0,
            "parameter_index": param_index,
            "parameter_type_display": prov.get("parameter_type_display") if prov else None,
            "direct_caller_count": direct_callers,
            "matched_caller_calls": matched_calls,
            "caller_entries": parse_json_strings(prov.get("caller_entries_json")) if prov else [],
            "caller_call_sites": parse_json_strings(prov.get("caller_call_sites_json")) if prov else [],
            "raw_candidate_types": sorted(set(raw_candidates)),
            "candidate_receiver_class_names": sorted(mapped),
            "affects_routing": False,
        })

    print("format: NEVERGONE.ReceiverParameterProvenanceAudit/1")
    print("inference rows:", len(inference_rows))
    print("parameter provenance rows:", len(provenance_rows))
    print("parameter provenance formats:", dict(formats.most_common()))
    print("unresolved rows:", len(joined))
    print("original status counts:", dict(original_statuses.most_common()))
    print("provenance status counts:", dict(provenance_statuses.most_common()))
    print("recovery status counts:", dict(recovery_statuses.most_common()))
    print("roots mapped to parameters:", root_parameter_rows)
    print("parameter index counts:", dict(sorted(parameter_indexes.items())))
    print("rows with direct callers:", rows_with_direct_callers)
    print("rows with matched caller calls:", rows_with_matched_calls)
    print("rows with caller type candidates:", rows_with_type_candidates)
    print("unique catalog recoveries:", recovery_statuses["unique_catalog_class"])
    print("top recovered classes:", dict(recovered_classes.most_common(20)))

    print("\nsamples:")
    for row in joined[: max(0, args.samples)]:
        print(
            row["source_id"],
            "call", row["call_index"],
            "original=", row["original_status"],
            "prov=", row["provenance_status"],
            "recovery=", row["recovery_status"],
            "param=", row["parameter_index"],
            "root_type=", row["root_type_display"],
            "callers=", row["direct_caller_count"],
            "matched=", row["matched_caller_calls"],
            "classes=", row["candidate_receiver_class_names"],
        )

    if args.details_output is not None:
        args.details_output.parent.mkdir(parents=True, exist_ok=True)
        with args.details_output.open("w", encoding="utf-8") as handle:
            for row in joined:
                handle.write(json.dumps(row, sort_keys=True) + "\n")
        print("details output:", args.details_output)

    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
