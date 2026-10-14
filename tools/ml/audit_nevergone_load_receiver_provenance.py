#!/usr/bin/env python3
"""Audit whether conservative LOAD provenance can recover unresolved receiver classes.

This diagnostic joins StaticReceiverVtableInference failures with evidence emitted by
ExportIndirectCallLoadReceiverProvenance.java and the static vtable catalog. It only
reports possible class recovery; it never changes inference or routing.
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
LOAD_FORMATS = {
    "NEVERGONE.GhidraLoadReceiverProvenance/1",
    "NEVERGONE.GhidraLoadReceiverProvenance/2",
}
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


def map_candidates(raw_candidates: list[str], catalog_classes: set[str],
                   terminal_to_classes: dict[str, set[str]]) -> set[str]:
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
    parser.add_argument("load_provenance_tsv", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    parser.add_argument("--samples", type=int, default=60)
    args = parser.parse_args()

    inference_rows = read_jsonl(args.inference_jsonl)
    load_rows = read_tsv(args.load_provenance_tsv)
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

    load_by_key: dict[tuple[str, int], dict[str, str]] = {}
    format_counts = Counter()
    for line_no, row in enumerate(load_rows, 2):
        fmt = row.get("format")
        if fmt not in LOAD_FORMATS:
            raise ValueError(f"{args.load_provenance_tsv}:{line_no}: expected one of {sorted(LOAD_FORMATS)}")
        format_counts[str(fmt)] += 1
        source_id = row.get("source_id")
        try:
            call_index = int(row.get("call_index", "-1"))
        except ValueError:
            continue
        if source_id and call_index >= 0:
            load_by_key[(source_id, call_index)] = row

    joined: list[dict[str, Any]] = []
    load_statuses = Counter()
    recovery_statuses = Counter()
    recovered_classes = Counter()
    original_statuses = Counter()
    pattern_counts = Counter()
    typed_field_rows = 0
    traced_rows = 0

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

        load = load_by_key.get((source_id, call_index))
        raw_candidates: list[str] = []
        mapped: set[str] = set()
        patterns: list[str] = []
        trace: list[str] = []
        typed_fields: list[str] = []

        if load is None:
            recovery_status = "missing_load_evidence"
            load_status = "<missing>"
        else:
            load_status = load.get("status") or "<missing>"
            load_statuses[load_status] += 1
            raw_candidates.extend(parse_json_strings(load.get("candidate_single_pointer_pointee_displays_json")))
            raw_candidates.extend(parse_json_strings(load.get("candidate_single_pointer_pointee_names_json")))
            mapped = map_candidates(raw_candidates, catalog_classes, terminal_to_classes)

            patterns = parse_json_strings(load.get("address_patterns_json"))
            if not patterns and load.get("address_pattern") not in (None, "", "-"):
                patterns = [str(load.get("address_pattern"))]
            for pattern in patterns:
                pattern_counts[pattern] += 1

            trace = parse_json_strings(load.get("provenance_trace_json"))
            typed_fields = parse_json_strings(load.get("typed_structure_fields_json"))
            if trace:
                traced_rows += 1
            if typed_fields:
                typed_field_rows += 1

            if load_status == "not_load":
                recovery_status = "receiver_not_load"
            elif not raw_candidates:
                recovery_status = "load_no_type_candidate"
            elif not mapped:
                recovery_status = "candidate_not_in_catalog"
            elif len(mapped) > 1:
                recovery_status = "ambiguous_catalog_class"
            else:
                recovery_status = "unique_catalog_class"
                recovered_classes[next(iter(mapped))] += 1

        recovery_statuses[recovery_status] += 1
        mapped_classes = sorted(mapped)

        joined.append({
            "source_id": source_id,
            "call_index": call_index,
            "call_address": inf.get("call_address"),
            "original_status": original_status,
            "dispatch_slot_offset": inf.get("dispatch_slot_offset"),
            "load_format": load.get("format") if load else None,
            "load_status": load_status,
            "recovery_status": recovery_status,
            "address_pattern": load.get("address_pattern") if load else None,
            "address_patterns": patterns,
            "provenance_node_count": load.get("provenance_node_count") if load else None,
            "provenance_trace": trace,
            "typed_structure_fields": typed_fields,
            "load_address_type_display": load.get("load_address_type_display") if load else None,
            "load_address_loaded_type_display": load.get("load_address_loaded_type_display") if load else None,
            "base_type_display": load.get("base_type_display") if load else None,
            "byte_offset": load.get("byte_offset") if load else None,
            "component_field_name": load.get("component_field_name") if load else None,
            "component_type_display": load.get("component_type_display") if load else None,
            "raw_candidate_types": sorted(set(raw_candidates)),
            "candidate_receiver_class_names": mapped_classes,
            "affects_routing": False,
        })

    print("format: NEVERGONE.LoadReceiverProvenanceAudit/2")
    print("inference rows:", len(inference_rows))
    print("load provenance rows:", len(load_rows))
    print("load provenance formats:", dict(format_counts.most_common()))
    print("unresolved rows:", len(joined))
    print("original status counts:", dict(original_statuses.most_common()))
    print("load status counts:", dict(load_statuses.most_common()))
    print("recovery status counts:", dict(recovery_statuses.most_common()))
    print("unique catalog recoveries:", recovery_statuses["unique_catalog_class"])
    print("top recovered classes:", dict(recovered_classes.most_common(20)))
    print("rows with recursive trace:", traced_rows)
    print("rows with typed structure fields:", typed_field_rows)
    print("address provenance patterns:", dict(pattern_counts.most_common()))

    print("\nsamples:")
    for row in joined[: max(0, args.samples)]:
        print(
            row["source_id"],
            "call", row["call_index"],
            "original=", row["original_status"],
            "load=", row["load_status"],
            "recovery=", row["recovery_status"],
            "patterns=", row["address_patterns"],
            "nodes=", row["provenance_node_count"],
            "offset=", row["byte_offset"],
            "component=", row["component_type_display"],
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
