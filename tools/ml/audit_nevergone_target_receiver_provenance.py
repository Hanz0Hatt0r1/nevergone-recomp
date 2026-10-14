#!/usr/bin/env python3
"""Audit structural receiver recovery from canonical CALLIND vtable data flow.

This is evidence-only. It validates recovered receiver varnodes against explicit
CALLIND first arguments where available, then measures conservative class recovery
for receiver_not_single_pointer rows using recovered local type or direct-caller
parameter type evidence. It never changes inference or routing.
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
PROVENANCE_FORMAT = "NEVERGONE.GhidraTargetReceiverProvenance/1"
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
    parser.add_argument("target_receiver_tsv", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    parser.add_argument("--samples", type=int, default=60)
    args = parser.parse_args()

    inference_rows = read_jsonl(args.inference_jsonl)
    provenance_rows = read_tsv(args.target_receiver_tsv)
    catalog_rows = read_jsonl(args.catalog_jsonl)

    catalog_classes: set[str] = set()
    for line_no, row in enumerate(catalog_rows, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{args.catalog_jsonl}:{line_no}: expected {CATALOG_FORMAT}")
        class_name = row.get("class_name")
        if isinstance(class_name, str) and class_name:
            catalog_classes.add(class_name)

    terminal_to_classes: dict[str, set[str]] = defaultdict(set)
    for class_name in catalog_classes:
        terminal_to_classes[terminal(class_name)].add(class_name)

    provenance_by_key: dict[tuple[str, int], dict[str, str]] = {}
    structural_statuses = Counter()
    canonical_rows = 0
    validation_rows = 0
    validation_matches = 0
    validation_mismatches = 0
    missing_direct_rows = 0
    missing_direct_canonical = 0

    for line_no, row in enumerate(provenance_rows, 2):
        if row.get("format") != PROVENANCE_FORMAT:
            raise ValueError(f"{args.target_receiver_tsv}:{line_no}: expected {PROVENANCE_FORMAT}")
        source_id = row.get("source_id")
        try:
            call_index = int(row.get("call_index", "-1"))
        except ValueError:
            continue
        if source_id and call_index >= 0:
            provenance_by_key[(source_id, call_index)] = row
        structural = row.get("structural_status") or "<missing>"
        structural_statuses[structural] += 1
        canonical = structural == "canonical_receiver"
        if canonical:
            canonical_rows += 1
        direct_present = row.get("direct_receiver_present") == "1"
        if direct_present:
            match = row.get("direct_receiver_matches_structural")
            if canonical and match in {"0", "1"}:
                validation_rows += 1
                if match == "1":
                    validation_matches += 1
                else:
                    validation_mismatches += 1
        else:
            missing_direct_rows += 1
            if canonical:
                missing_direct_canonical += 1

    target_rows = [row for row in inference_rows if row.get("status") == "receiver_not_single_pointer"]
    recovery_statuses = Counter()
    recovered_classes = Counter()
    joined: list[dict[str, Any]] = []
    structural_candidates = 0
    direct_pointer_candidates = 0
    parameter_roots = 0
    rows_with_caller_candidates = 0
    direct_unique = 0
    caller_unique = 0

    for inf in target_rows:
        if inf.get("format") != INFERENCE_FORMAT:
            raise ValueError(f"expected {INFERENCE_FORMAT}")
        source_id = inf.get("source_id")
        call_index = inf.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            continue
        prov = provenance_by_key.get((source_id, call_index))
        raw_direct: list[str] = []
        raw_caller: list[str] = []
        mapped_direct: set[str] = set()
        mapped_caller: set[str] = set()
        param_index = -1

        if prov is None:
            status = "missing_target_provenance"
        elif prov.get("structural_status") != "canonical_receiver":
            status = "no_canonical_structural_receiver"
        else:
            structural_candidates += 1
            pointer_depth = parse_int(prov.get("recovered_receiver_pointer_depth"), -1)
            pointee = clean_type(prov.get("recovered_pointee_type_display"))
            if pointer_depth == 1 and pointee is not None:
                direct_pointer_candidates += 1
                raw_direct.append(pointee)
                mapped_direct = map_candidates(raw_direct, catalog_classes, terminal_to_classes)

            if len(mapped_direct) == 1:
                status = "unique_catalog_class_direct"
                cls = next(iter(mapped_direct))
                recovered_classes[cls] += 1
                direct_unique += 1
            elif len(mapped_direct) > 1:
                status = "ambiguous_catalog_class_direct"
            else:
                param_index = parse_int(prov.get("parameter_index"), -1)
                if param_index >= 0:
                    parameter_roots += 1
                raw_caller.extend(parse_json_strings(prov.get("caller_single_pointer_pointee_displays_json")))
                raw_caller.extend(parse_json_strings(prov.get("caller_single_pointer_pointee_names_json")))
                if raw_caller:
                    rows_with_caller_candidates += 1
                mapped_caller = map_candidates(raw_caller, catalog_classes, terminal_to_classes)
                if len(mapped_caller) == 1:
                    status = "unique_catalog_class_caller"
                    cls = next(iter(mapped_caller))
                    recovered_classes[cls] += 1
                    caller_unique += 1
                elif len(mapped_caller) > 1:
                    status = "ambiguous_catalog_class_caller"
                elif param_index < 0:
                    status = "structural_receiver_root_not_parameter"
                elif not raw_caller:
                    status = "caller_no_type_candidate"
                else:
                    status = "caller_candidate_not_in_catalog"

        recovery_statuses[status] += 1
        joined.append({
            "source_id": source_id,
            "call_index": call_index,
            "call_address": inf.get("call_address"),
            "dispatch_slot_offset": inf.get("dispatch_slot_offset"),
            "structural_status": prov.get("structural_status") if prov else None,
            "target_pattern": prov.get("target_pattern") if prov else None,
            "target_slot_offset": prov.get("target_slot_offset") if prov else None,
            "recovered_receiver_token": prov.get("recovered_receiver_token") if prov else None,
            "recovered_receiver_type_display": prov.get("recovered_receiver_type_display") if prov else None,
            "recovered_receiver_pointer_depth": parse_int(prov.get("recovered_receiver_pointer_depth"), -1) if prov else -1,
            "recovered_pointee_type_display": prov.get("recovered_pointee_type_display") if prov else None,
            "root_token": prov.get("root_token") if prov else None,
            "root_type_display": prov.get("root_type_display") if prov else None,
            "parameter_index": param_index,
            "matched_caller_calls": parse_int(prov.get("matched_caller_calls")) if prov else 0,
            "raw_direct_candidates": sorted(set(raw_direct)),
            "raw_caller_candidates": sorted(set(raw_caller)),
            "candidate_receiver_class_names": sorted(mapped_direct or mapped_caller),
            "recovery_status": status,
            "affects_routing": False,
        })

    print("format: NEVERGONE.TargetReceiverProvenanceAudit/1")
    print("inference rows:", len(inference_rows))
    print("target receiver rows:", len(provenance_rows))
    print("structural status counts:", dict(structural_statuses.most_common()))
    print("canonical structural receivers:", canonical_rows)
    print("explicit-receiver validation rows:", validation_rows)
    print("explicit-receiver validation matches:", validation_matches)
    print("explicit-receiver validation mismatches:", validation_mismatches)
    print("missing explicit receiver rows:", missing_direct_rows)
    print("missing explicit receiver with canonical structural receiver:", missing_direct_canonical)
    print("receiver_not_single_pointer rows:", len(target_rows))
    print("structural candidates for missing receivers:", structural_candidates)
    print("single-pointer structural type candidates:", direct_pointer_candidates)
    print("structural roots mapped to parameters:", parameter_roots)
    print("rows with caller type candidates:", rows_with_caller_candidates)
    print("recovery status counts:", dict(recovery_statuses.most_common()))
    print("unique catalog recoveries direct:", direct_unique)
    print("unique catalog recoveries caller:", caller_unique)
    print("unique catalog recoveries total:", direct_unique + caller_unique)
    print("top recovered classes:", dict(recovered_classes.most_common(20)))

    print("\nsamples:")
    for row in joined[: max(0, args.samples)]:
        print(
            row["source_id"],
            "call", row["call_index"],
            "slot=", row["dispatch_slot_offset"],
            "structural=", row["structural_status"],
            "target_pattern=", row["target_pattern"],
            "target_offset=", row["target_slot_offset"],
            "receiver_type=", row["recovered_receiver_type_display"],
            "pointee=", row["recovered_pointee_type_display"],
            "param=", row["parameter_index"],
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
