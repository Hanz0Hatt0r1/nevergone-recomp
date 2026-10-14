#!/usr/bin/env python3
"""Audit caller-side class recovery for structurally recovered CALLIND receivers."""
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

INFERENCE_FORMAT = "NEVERGONE.StaticReceiverVtableInference/1"
PROVENANCE_FORMAT = "NEVERGONE.GhidraStructuralReceiverParameterProvenance/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"


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


def parse_int(value: Any, default: int = 0) -> int:
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
    return [item for item in parsed if isinstance(item, str) and item]


def map_candidates(raw: list[str], catalog: set[str], by_terminal: dict[str, set[str]]) -> set[str]:
    mapped: set[str] = set()
    for candidate in raw:
        cleaned = clean_type(candidate)
        if cleaned is None:
            continue
        if cleaned in catalog:
            mapped.add(cleaned)
            continue
        matches = by_terminal.get(terminal(cleaned), set())
        if len(matches) == 1:
            mapped.update(matches)
    return mapped


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("provenance_tsv", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    parser.add_argument("--samples", type=int, default=60)
    args = parser.parse_args()

    inference = read_jsonl(args.inference_jsonl)
    provenance = read_tsv(args.provenance_tsv)
    catalog_rows = read_jsonl(args.catalog_jsonl)

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

    prov_by_key: dict[tuple[str, int], dict[str, str]] = {}
    statuses = Counter()
    for line_no, row in enumerate(provenance, 2):
        if row.get("format") != PROVENANCE_FORMAT:
            raise ValueError(f"{args.provenance_tsv}:{line_no}: expected {PROVENANCE_FORMAT}")
        source_id = row.get("source_id")
        try:
            call_index = int(row.get("call_index", "-1"))
        except ValueError:
            continue
        if source_id and call_index >= 0:
            prov_by_key[(source_id, call_index)] = row
        statuses[row.get("status") or "<missing>"] += 1

    targets = [row for row in inference if row.get("status") == "receiver_not_single_pointer"]
    joined: list[dict[str, Any]] = []
    recovery = Counter()
    recovered_classes = Counter()
    structural = 0
    load_receivers = 0
    parameter_roots = 0
    matched_caller_rows = 0
    caller_type_rows = 0
    unique = 0
    ambiguous = 0

    for inf in targets:
        if inf.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"expected {INFERENCE_FORMAT}")
        source_id = inf.get("source_id")
        call_index = inf.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            continue
        prov = prov_by_key.get((source_id, call_index))
        raw: list[str] = []
        mapped: set[str] = set()
        if prov is None:
            status = "missing_provenance"
        else:
            if prov.get("structural_receiver_present") == "1":
                structural += 1
            if prov.get("structural_receiver_is_load") == "1":
                load_receivers += 1
            param_index = parse_int(prov.get("parameter_index"), -1)
            if param_index >= 0:
                parameter_roots += 1
            matched_calls = parse_int(prov.get("matched_caller_calls"), 0)
            if matched_calls > 0:
                matched_caller_rows += 1
            raw.extend(parse_json_strings(prov.get("caller_single_pointer_pointee_displays_json")))
            raw.extend(parse_json_strings(prov.get("caller_single_pointer_pointee_names_json")))
            if raw:
                caller_type_rows += 1
            mapped = map_candidates(raw, catalog, by_terminal)

            if len(mapped) == 1:
                status = "unique_catalog_class"
                cls = next(iter(mapped))
                recovered_classes[cls] += 1
                unique += 1
            elif len(mapped) > 1:
                status = "ambiguous_catalog_class"
                ambiguous += 1
            else:
                source_status = prov.get("status") or "<missing>"
                if source_status == "no_canonical_structural_receiver":
                    status = source_status
                elif source_status == "structural_receiver_not_load":
                    status = source_status
                elif param_index < 0:
                    status = "load_address_root_not_parameter"
                elif matched_calls == 0:
                    status = "parameter_no_direct_caller_calls"
                elif not raw:
                    status = "caller_no_single_pointer_type"
                else:
                    status = "caller_candidate_not_in_catalog"

        recovery[status] += 1
        joined.append({
            "source_id": source_id,
            "call_index": call_index,
            "call_address": inf.get("call_address"),
            "dispatch_slot_offset": inf.get("dispatch_slot_offset"),
            "provenance_status": prov.get("status") if prov else None,
            "structural_receiver_token": prov.get("structural_receiver_token") if prov else None,
            "structural_receiver_type_display": prov.get("structural_receiver_type_display") if prov else None,
            "structural_receiver_is_load": prov.get("structural_receiver_is_load") if prov else None,
            "load_address_token": prov.get("load_address_token") if prov else None,
            "root_token": prov.get("root_token") if prov else None,
            "root_type_display": prov.get("root_type_display") if prov else None,
            "root_trace_patterns": parse_json_strings(prov.get("root_trace_patterns_json")) if prov else [],
            "parameter_index": parse_int(prov.get("parameter_index"), -1) if prov else -1,
            "matched_caller_calls": parse_int(prov.get("matched_caller_calls"), 0) if prov else 0,
            "raw_caller_candidates": sorted(set(raw)),
            "candidate_receiver_class_names": sorted(mapped),
            "recovery_status": status,
            "affects_routing": False,
        })

    print("format: NEVERGONE.StructuralReceiverParameterProvenanceAudit/1")
    print("inference rows:", len(inference))
    print("provenance rows:", len(provenance))
    print("receiver_not_single_pointer rows:", len(targets))
    print("exporter status counts:", dict(statuses.most_common()))
    print("structural receivers in target rows:", structural)
    print("structural LOAD receivers in target rows:", load_receivers)
    print("LOAD-address roots mapped to parameters:", parameter_roots)
    print("rows with matched direct-caller calls:", matched_caller_rows)
    print("rows with caller type candidates:", caller_type_rows)
    print("recovery status counts:", dict(recovery.most_common()))
    print("unique catalog recoveries:", unique)
    print("ambiguous catalog recoveries:", ambiguous)
    print("top recovered classes:", dict(recovered_classes.most_common(20)))

    print("\nsamples:")
    for row in joined[: max(0, args.samples)]:
        print(
            row["source_id"],
            "call", row["call_index"],
            "slot=", row["dispatch_slot_offset"],
            "prov=", row["provenance_status"],
            "receiver_type=", row["structural_receiver_type_display"],
            "root_type=", row["root_type_display"],
            "param=", row["parameter_index"],
            "matched=", row["matched_caller_calls"],
            "recovery=", row["recovery_status"],
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
