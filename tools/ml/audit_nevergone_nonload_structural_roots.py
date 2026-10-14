#!/usr/bin/env python3
"""Audit root/caller provenance for receiver_not_single_pointer rows whose canonical structural receiver is non-LOAD.

Evidence-only. Reuses the already exported target receiver provenance, which traces the recovered structural
receiver itself regardless of whether that receiver is produced by LOAD. No inference/query/routing changes.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

SHAPE_FORMAT = "NEVERGONE.MissingReceiverShapeAudit/1"
TARGET_FORMAT = "NEVERGONE.GhidraTargetReceiverProvenance/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
AUDIT_FORMAT = "NEVERGONE.NonLoadStructuralRootAudit/1"


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
    while text.endswith(" *"):
        text = text[:-2].rstrip()
    return text or None


def terminal(name: str) -> str:
    return name.rsplit("::", 1)[-1]


def map_candidates(values: list[str], catalog: set[str], by_terminal: dict[str, set[str]]) -> set[str]:
    result: set[str] = set()
    for value in values:
        cleaned = clean_type(value)
        if cleaned is None:
            continue
        if cleaned in catalog:
            result.add(cleaned)
            continue
        matches = by_terminal.get(terminal(cleaned), set())
        if len(matches) == 1:
            result.update(matches)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("shape_audit_jsonl", type=Path)
    parser.add_argument("target_provenance_tsv", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    shape_rows = read_jsonl(args.shape_audit_jsonl)
    targets = read_tsv(args.target_provenance_tsv)
    catalog_rows = read_jsonl(args.catalog_jsonl)

    target_by_key: dict[tuple[str, int], dict[str, str]] = {}
    for line_no, row in enumerate(targets, 2):
        if row.get("format") != TARGET_FORMAT:
            raise ValueError(f"{args.target_provenance_tsv}:{line_no}: expected {TARGET_FORMAT}")
        source_id = row.get("source_id")
        call_index = parse_int(row.get("call_index"), -1)
        if source_id and call_index >= 0:
            target_by_key[(source_id, call_index)] = row

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

    selected: list[dict[str, Any]] = []
    for line_no, row in enumerate(shape_rows, 1):
        if row.get("format") != SHAPE_FORMAT:
            raise ValueError(f"{args.shape_audit_jsonl}:{line_no}: expected {SHAPE_FORMAT}")
        if row.get("bucket") == "structural_receiver_not_load":
            selected.append(row)

    buckets = Counter()
    details: list[dict[str, Any]] = []
    parameter_roots = matched_rows = caller_unique = local_unique = 0

    for shape in selected:
        source_id = shape.get("source_id")
        call_index = shape.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            continue
        target = target_by_key.get((source_id, call_index))
        if target is None:
            bucket = "missing_target_provenance"
            root_token = None
            param_index = -1
            matched = 0
            caller_raw: list[str] = []
            local_raw: list[str] = []
        else:
            root_token = target.get("root_token")
            param_index = parse_int(target.get("parameter_index"), -1)
            matched = parse_int(target.get("matched_caller_calls"), 0)
            caller_raw = (
                parse_json_strings(target.get("caller_single_pointer_pointee_displays_json"))
                + parse_json_strings(target.get("caller_single_pointer_pointee_names_json"))
            )
            local_raw = [
                target.get("recovered_pointee_type_display") or "",
                target.get("root_type_display") or "",
                target.get("parameter_type_display") or "",
            ]
            local_classes = map_candidates(local_raw, catalog, by_terminal)
            caller_classes = map_candidates(caller_raw, catalog, by_terminal)
            if len(local_classes) == 1:
                local_unique += 1
            if param_index >= 0:
                parameter_roots += 1
            if matched > 0:
                matched_rows += 1
            if len(caller_classes) == 1:
                caller_unique += 1

            if target.get("structural_status") != "canonical_receiver":
                bucket = "target_not_canonical"
            elif len(local_classes) == 1:
                bucket = "unique_local_root_candidate"
            elif len(local_classes) > 1:
                bucket = "ambiguous_local_root_candidate"
            elif param_index < 0:
                bucket = "receiver_root_not_parameter"
            elif matched == 0:
                bucket = "parameter_no_direct_caller_calls"
            elif len(caller_classes) == 1:
                bucket = "unique_caller_catalog_candidate"
            elif len(caller_classes) > 1:
                bucket = "ambiguous_caller_catalog_candidate"
            elif caller_raw:
                bucket = "caller_type_not_in_catalog"
            else:
                bucket = "caller_calls_no_single_pointer_type"

        local_classes = map_candidates(local_raw, catalog, by_terminal) if target else set()
        caller_classes = map_candidates(caller_raw, catalog, by_terminal) if target else set()
        buckets[bucket] += 1
        details.append({
            "format": AUDIT_FORMAT,
            "source_id": source_id,
            "call_index": call_index,
            "call_address": shape.get("call_address"),
            "dispatch_slot_offset": shape.get("dispatch_slot_offset"),
            "root_token": root_token,
            "root_type_display": target.get("root_type_display") if target else None,
            "root_trace_patterns": parse_json_strings(target.get("root_trace_patterns_json")) if target else [],
            "root_trace_nodes": parse_int(target.get("root_trace_nodes"), 0) if target else 0,
            "parameter_index": param_index,
            "parameter_type_display": target.get("parameter_type_display") if target else None,
            "direct_caller_count": parse_int(target.get("direct_caller_count"), 0) if target else 0,
            "matched_caller_calls": matched,
            "raw_local_types": sorted({x for x in local_raw if x}),
            "mapped_local_classes": sorted(local_classes),
            "raw_caller_types": sorted(set(caller_raw)),
            "mapped_caller_classes": sorted(caller_classes),
            "bucket": bucket,
            "affects_routing": False,
        })

    print("format:", AUDIT_FORMAT)
    print("structural_receiver_not_load rows:", len(selected))
    print("receiver roots mapped to parameters:", parameter_roots)
    print("rows with matched direct-caller calls:", matched_rows)
    print("unique local/root catalog candidates:", local_unique)
    print("unique caller catalog candidates:", caller_unique)
    print("bucket counts:", dict(buckets.most_common()))
    print("\nrows:")
    for row in details:
        print(
            row["source_id"], "call", row["call_index"],
            "slot=", row["dispatch_slot_offset"],
            "root=", row["root_token"],
            "root_type=", row["root_type_display"],
            "trace=", row["root_trace_patterns"],
            "param=", row["parameter_index"],
            "matched=", row["matched_caller_calls"],
            "local_classes=", row["mapped_local_classes"],
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
