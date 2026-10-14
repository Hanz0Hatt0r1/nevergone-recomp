#!/usr/bin/env python3
"""Classify unresolved receiver_not_single_pointer CALLIND rows by evidence stage.

Evidence-only. Joins v6 inference with canonical target/slot provenance and the
structural receiver parameter/caller provenance. It identifies the earliest stage
where each unresolved receiver loses usable class evidence. It never creates a
vtable query and never changes routing.
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
STRUCTURAL_FORMAT = "NEVERGONE.GhidraStructuralReceiverParameterProvenance/1"
TARGET_FORMAT = "NEVERGONE.GhidraTargetReceiverProvenance/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
AUDIT_FORMAT = "NEVERGONE.MissingReceiverShapeAudit/1"


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


def parse_int(value: Any, default: int = -1) -> int:
    if value in (None, "", "-"):
        return default
    try:
        return int(str(value), 0)
    except ValueError:
        return default


def normalize_slot(value: Any) -> int | None:
    parsed = parse_int(value, -1)
    return parsed if parsed >= 0 else None


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


def map_candidates(values: list[str], catalog: set[str], by_terminal: dict[str, set[str]]) -> set[str]:
    mapped: set[str] = set()
    for value in values:
        cleaned = clean_type(value)
        if cleaned is None:
            continue
        if cleaned in catalog:
            mapped.add(cleaned)
            continue
        matches = by_terminal.get(terminal(cleaned), set())
        if len(matches) == 1:
            mapped.update(matches)
    return mapped


def keyed(rows: list[dict[str, str]], *, path: Path, expected_format: str) -> dict[tuple[str, int], dict[str, str]]:
    result: dict[tuple[str, int], dict[str, str]] = {}
    for line_no, row in enumerate(rows, 2):
        if row.get("format") != expected_format:
            raise ValueError(f"{path}:{line_no}: expected {expected_format}")
        source_id = row.get("source_id")
        call_index = parse_int(row.get("call_index"), -1)
        if not source_id or call_index < 0:
            continue
        key = (source_id, call_index)
        if key in result:
            raise ValueError(f"{path}:{line_no}: duplicate key {key}")
        result[key] = row
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("structural_provenance_tsv", type=Path)
    parser.add_argument("target_provenance_tsv", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    inference = read_jsonl(args.inference_jsonl)
    structural_rows = read_tsv(args.structural_provenance_tsv)
    target_rows = read_tsv(args.target_provenance_tsv)
    catalog_rows = read_jsonl(args.catalog_jsonl)

    structural_by_key = keyed(
        structural_rows, path=args.structural_provenance_tsv, expected_format=STRUCTURAL_FORMAT
    )
    target_by_key = keyed(target_rows, path=args.target_provenance_tsv, expected_format=TARGET_FORMAT)

    catalog: set[str] = set()
    for line_no, row in enumerate(catalog_rows, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{args.catalog_jsonl}:{line_no}: expected {CATALOG_FORMAT}")
        name = row.get("class_name")
        if isinstance(name, str) and name:
            catalog.add(name)
    by_terminal: dict[str, set[str]] = defaultdict(set)
    for name in catalog:
        by_terminal[terminal(name)].add(name)

    selected = [
        row for row in inference
        if row.get("format") == INFERENCE_FORMAT and row.get("status") == "receiver_not_single_pointer"
    ]

    buckets = Counter()
    details: list[dict[str, Any]] = []
    canonical = 0
    slot_matches = 0
    structural_present = 0
    structural_typed = 0
    structural_load = 0
    parameter_roots = 0
    caller_rows = 0
    local_unique = 0
    caller_unique = 0

    for inf in selected:
        source_id = inf.get("source_id")
        call_index = inf.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            continue
        key = (source_id, call_index)
        structural = structural_by_key.get(key)
        target = target_by_key.get(key)

        existing_slot = normalize_slot(inf.get("dispatch_slot_offset"))
        target_slot = normalize_slot(target.get("target_slot_offset")) if target else None
        is_canonical = bool(target and target.get("structural_status") == "canonical_receiver")
        slot_match = is_canonical and existing_slot is not None and target_slot == existing_slot
        if is_canonical:
            canonical += 1
        if slot_match:
            slot_matches += 1

        structural_is_present = bool(structural and structural.get("structural_receiver_present") == "1")
        if structural_is_present:
            structural_present += 1
        structural_depth = parse_int(structural.get("structural_receiver_pointer_depth"), 0) if structural else 0
        structural_pointee = structural.get("structural_receiver_pointee_type_display") if structural else None
        local_classes = map_candidates([structural_pointee] if structural_pointee else [], catalog, by_terminal)
        if structural_depth == 1:
            structural_typed += 1
        if len(local_classes) == 1:
            local_unique += 1

        is_load = bool(structural and structural.get("structural_receiver_is_load") == "1")
        if is_load:
            structural_load += 1
        param_index = parse_int(structural.get("parameter_index"), -1) if structural else -1
        if param_index >= 0:
            parameter_roots += 1
        matched_calls = parse_int(structural.get("matched_caller_calls"), 0) if structural else 0
        if matched_calls > 0:
            caller_rows += 1
        raw_callers: list[str] = []
        if structural:
            raw_callers += parse_json_strings(structural.get("caller_single_pointer_pointee_displays_json"))
            raw_callers += parse_json_strings(structural.get("caller_single_pointer_pointee_names_json"))
        caller_classes = map_candidates(raw_callers, catalog, by_terminal)
        if len(caller_classes) == 1:
            caller_unique += 1

        if structural is None or target is None:
            bucket = "missing_provenance"
        elif not is_canonical:
            bucket = "no_canonical_structural_receiver"
        elif not slot_match:
            bucket = "canonical_slot_mismatch"
        elif not structural_is_present:
            bucket = "structural_receiver_not_exported"
        elif len(local_classes) == 1:
            bucket = "unique_local_structural_type_candidate"
        elif len(local_classes) > 1:
            bucket = "ambiguous_local_structural_type"
        elif not is_load:
            bucket = "structural_receiver_not_load"
        elif param_index < 0:
            bucket = "load_address_root_not_parameter"
        elif matched_calls == 0:
            bucket = "parameter_no_direct_caller_calls"
        elif len(caller_classes) == 1:
            bucket = "unique_caller_catalog_candidate"
        elif len(caller_classes) > 1:
            bucket = "ambiguous_caller_catalog_candidate"
        elif raw_callers:
            bucket = "caller_type_not_in_catalog"
        else:
            bucket = "caller_calls_no_single_pointer_type"
        buckets[bucket] += 1

        details.append({
            "format": AUDIT_FORMAT,
            "source_id": source_id,
            "call_index": call_index,
            "call_address": inf.get("call_address"),
            "dispatch_slot_offset": inf.get("dispatch_slot_offset"),
            "target_structural_status": target.get("structural_status") if target else None,
            "target_pattern": target.get("target_pattern") if target else None,
            "target_slot_offset": target.get("target_slot_offset") if target else None,
            "target_slot_matches_inference": slot_match,
            "structural_provenance_status": structural.get("status") if structural else None,
            "structural_receiver_token": structural.get("structural_receiver_token") if structural else None,
            "structural_receiver_type_display": structural.get("structural_receiver_type_display") if structural else None,
            "structural_receiver_pointer_depth": structural_depth,
            "structural_receiver_pointee_type_display": structural_pointee,
            "mapped_local_structural_classes": sorted(local_classes),
            "structural_receiver_is_load": is_load,
            "load_address_token": structural.get("load_address_token") if structural else None,
            "root_token": structural.get("root_token") if structural else None,
            "root_type_display": structural.get("root_type_display") if structural else None,
            "root_trace_patterns": parse_json_strings(structural.get("root_trace_patterns_json")) if structural else [],
            "parameter_index": param_index,
            "parameter_type_display": structural.get("parameter_type_display") if structural else None,
            "direct_caller_count": parse_int(structural.get("direct_caller_count"), 0) if structural else 0,
            "matched_caller_calls": matched_calls,
            "raw_caller_types": sorted(set(raw_callers)),
            "mapped_caller_classes": sorted(caller_classes),
            "bucket": bucket,
            "affects_routing": False,
        })

    print("format:", AUDIT_FORMAT)
    print("inference rows:", len(inference))
    print("receiver_not_single_pointer rows:", len(selected))
    print("canonical target rows:", canonical)
    print("canonical target rows with exact slot match:", slot_matches)
    print("structural receiver rows:", structural_present)
    print("structural receiver pointer-depth-1 rows:", structural_typed)
    print("structural LOAD receiver rows:", structural_load)
    print("LOAD-address roots mapped to parameters:", parameter_roots)
    print("rows with matched direct-caller calls:", caller_rows)
    print("unique local structural class candidates:", local_unique)
    print("unique caller catalog candidates:", caller_unique)
    print("bucket counts:", dict(buckets.most_common()))

    print("\nrows:")
    for row in details:
        print(
            row["source_id"], "call", row["call_index"],
            "slot=", row["dispatch_slot_offset"],
            "target=", row["target_structural_status"],
            "slot_match=", row["target_slot_matches_inference"],
            "struct_type=", row["structural_receiver_type_display"],
            "struct_depth=", row["structural_receiver_pointer_depth"],
            "load=", row["structural_receiver_is_load"],
            "root=", row["root_token"],
            "param=", row["parameter_index"],
            "matched=", row["matched_caller_calls"],
            "local_classes=", row["mapped_local_structural_classes"],
            "caller_classes=", row["mapped_caller_classes"],
            "bucket=", row["bucket"],
        )

    if args.details_output is not None:
        args.details_output.parent.mkdir(parents=True, exist_ok=True)
        with args.details_output.open("w", encoding="utf-8") as handle:
            for row in details:
                handle.write(json.dumps(row, sort_keys=True) + "\n")
        print("details output:", args.details_output)

    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
