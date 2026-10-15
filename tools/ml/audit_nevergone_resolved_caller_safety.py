#!/usr/bin/env python3
"""Audit safety gates for receiver classes recovered from caller argument types.

Caller argument types are class evidence only when the receiver itself is proven
to root at the same function parameter. A LOAD from a parameter-relative field
is not the parameter object and must not inherit the owner's class.
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
COMPLETENESS_FORMAT = "NEVERGONE.GhidraResolvedCallerTypeCompleteness/1"
TARGET_FORMAT = "NEVERGONE.GhidraTargetReceiverProvenance/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
CALLER_SOURCES = {"parameter_caller_type", "structural_parameter_caller_type"}
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
    return [x for x in parsed if isinstance(x, str) and x]


def clean_type(value: Any) -> str | None:
    if not isinstance(value, str):
        return None
    text = re.sub(r"^(?:class|struct)\s+", "", value.strip())
    text = re.sub(r"\s+", " ", text).strip()
    return text or None


def terminal(value: str) -> str:
    return value.rsplit("::", 1)[-1]


def normalize_slot(value: Any) -> str | None:
    if value in (None, "", "-"):
        return None
    try:
        parsed = int(str(value), 0)
    except ValueError:
        return None
    return f"0x{parsed:x}" if parsed >= 0 else None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inference_jsonl", type=Path)
    parser.add_argument("caller_completeness_tsv", type=Path)
    parser.add_argument("target_receiver_tsv", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    inference = read_jsonl(args.inference_jsonl)
    completeness = read_tsv(args.caller_completeness_tsv)
    targets = read_tsv(args.target_receiver_tsv)
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

    complete_by_key: dict[tuple[str, int], dict[str, str]] = {}
    for line_no, row in enumerate(completeness, 2):
        if row.get("format") != COMPLETENESS_FORMAT:
            raise ValueError(f"{args.caller_completeness_tsv}:{line_no}: expected {COMPLETENESS_FORMAT}")
        source_id = row.get("source_id")
        try:
            call_index = int(row.get("call_index", "-1"))
        except ValueError:
            continue
        if source_id and call_index >= 0:
            complete_by_key[(source_id, call_index)] = row

    target_by_key: dict[tuple[str, int], dict[str, str]] = {}
    for line_no, row in enumerate(targets, 2):
        if row.get("format") != TARGET_FORMAT:
            raise ValueError(f"{args.target_receiver_tsv}:{line_no}: expected {TARGET_FORMAT}")
        source_id = row.get("source_id")
        try:
            call_index = int(row.get("call_index", "-1"))
        except ValueError:
            continue
        if source_id and call_index >= 0:
            key = (source_id, call_index)
            if key in target_by_key:
                raise ValueError(f"duplicate target receiver provenance row: {key}")
            target_by_key[key] = row

    selected = [
        row for row in inference
        if row.get("format") == INFERENCE_FORMAT
        and row.get("status") in RESOLVED
        and row.get("class_evidence_source") in CALLER_SOURCES
    ]

    details: list[dict[str, Any]] = []
    statuses = Counter()
    complete_typed = 0
    class_convergent = 0
    parameter_root_matches = 0
    target_slot_matches = 0
    direct_receiver_matches = 0
    safe = 0

    for inf in selected:
        source_id = inf.get("source_id")
        call_index = inf.get("call_index")
        if not isinstance(source_id, str) or not isinstance(call_index, int):
            continue
        key = (source_id, call_index)
        comp = complete_by_key.get(key)
        target = target_by_key.get(key)
        source_kind = inf.get("class_evidence_source")
        inferred_class = inf.get("class_name")

        reasons: list[str] = []
        if comp is None:
            reasons.append("missing_caller_completeness")
            mapped: set[str] = set()
        else:
            if comp.get("status") != "complete_typed_callers":
                reasons.append(comp.get("status") or "caller_completeness_failed")
            else:
                complete_typed += 1
            raw = parse_json_strings(comp.get("caller_single_pointer_pointee_displays_json"))
            raw += parse_json_strings(comp.get("caller_single_pointer_pointee_names_json"))
            mapped = set()
            for value in raw:
                cleaned = clean_type(value)
                if cleaned is None:
                    continue
                if cleaned in catalog:
                    mapped.add(cleaned)
                else:
                    matches = by_terminal.get(terminal(cleaned), set())
                    if len(matches) == 1:
                        mapped.update(matches)
            if mapped == {inferred_class}:
                class_convergent += 1
            else:
                reasons.append("caller_classes_do_not_converge_to_inferred_class")

        target_status = target.get("structural_status") if target else None
        target_parameter_index = parse_int(target.get("parameter_index"), -1) if target else -1
        target_parameter_type = target.get("parameter_type_display") if target else None
        target_slot = normalize_slot(target.get("target_slot_offset")) if target else None
        existing_slot = normalize_slot(inf.get("dispatch_slot_offset"))
        target_call_address = target.get("call_address") if target else None
        direct_present = target.get("direct_receiver_present") if target else None
        direct_match = target.get("direct_receiver_matches_structural") if target else None

        canonical = bool(target and target_status == "canonical_receiver")
        parameter_root_match = canonical and target_parameter_index >= 0
        slot_match = canonical and existing_slot is not None and target_slot == existing_slot
        call_match = bool(target and target_call_address == inf.get("call_address"))
        explicit_receiver_match = direct_present != "1" or direct_match == "1"

        if target is None:
            reasons.append("missing_target_receiver_provenance")
        else:
            if not canonical:
                reasons.append("target_receiver_not_canonical")
            if not call_match:
                reasons.append("target_receiver_call_address_mismatch")
            if not slot_match:
                reasons.append("target_receiver_slot_mismatch")
            if not parameter_root_match:
                reasons.append("receiver_not_parameter_root")
            if not explicit_receiver_match:
                reasons.append("direct_receiver_structural_mismatch")

        if parameter_root_match:
            parameter_root_matches += 1
        if slot_match:
            target_slot_matches += 1
        if direct_present == "1" and direct_match == "1":
            direct_receiver_matches += 1

        if not reasons:
            status = "safe_caller_recovery"
            safe += 1
        else:
            status = "unsafe_caller_recovery"
        statuses[status] += 1
        details.append({
            "format": "NEVERGONE.ResolvedCallerSafetyAudit/1",
            "source_id": source_id,
            "call_index": call_index,
            "call_address": inf.get("call_address"),
            "class_evidence_source": source_kind,
            "inferred_class_name": inferred_class,
            "dispatch_slot_offset": inf.get("dispatch_slot_offset"),
            "caller_completeness_status": comp.get("status") if comp else None,
            "direct_caller_functions": parse_int(comp.get("direct_caller_functions")) if comp else 0,
            "caller_functions_decompiled": parse_int(comp.get("caller_functions_decompiled")) if comp else 0,
            "caller_functions_failed": parse_int(comp.get("caller_functions_failed")) if comp else 0,
            "matched_caller_calls": parse_int(comp.get("matched_caller_calls")) if comp else 0,
            "typed_caller_calls": parse_int(comp.get("typed_caller_calls")) if comp else 0,
            "untyped_caller_calls": parse_int(comp.get("untyped_caller_calls")) if comp else 0,
            "mapped_caller_classes": sorted(mapped),
            "target_structural_status": target_status,
            "target_parameter_index": target_parameter_index,
            "target_parameter_type_display": target_parameter_type,
            "target_slot_offset": target_slot,
            "target_call_address_match": call_match,
            "receiver_roots_at_parameter": parameter_root_match,
            "structural_slot_match": slot_match,
            "direct_receiver_matches_structural": direct_match,
            "reasons": reasons,
            "status": status,
            "affects_routing": False,
        })

    print("format: NEVERGONE.ResolvedCallerSafetyAudit/1")
    print("inference rows:", len(inference))
    print("caller-derived resolved rows:", len(selected))
    print("complete typed caller rows:", complete_typed)
    print("caller class convergence rows:", class_convergent)
    print("receiver parameter-root matches:", parameter_root_matches)
    print("target slot matches:", target_slot_matches)
    print("explicit receiver structural matches:", direct_receiver_matches)
    print("safe caller recoveries:", safe)
    print("unsafe caller recoveries:", len(selected) - safe)
    print("status counts:", dict(statuses))
    print("safety gate:", "pass" if selected and safe == len(selected) else "fail")

    print("\nrows:")
    for row in details:
        print(
            row["source_id"], "call", row["call_index"],
            "source=", row["class_evidence_source"],
            "class=", row["inferred_class_name"],
            "callers=", row["direct_caller_functions"],
            "matched=", row["matched_caller_calls"],
            "typed=", row["typed_caller_calls"],
            "parameter=", row["target_parameter_index"],
            "slot_match=", row["structural_slot_match"],
            "status=", row["status"],
            "reasons=", row["reasons"],
        )

    if args.details_output is not None:
        args.details_output.parent.mkdir(parents=True, exist_ok=True)
        with args.details_output.open("w", encoding="utf-8") as handle:
            for row in details:
                handle.write(json.dumps(row, sort_keys=True) + "\n")
        print("details output:", args.details_output)

    print("affects routing: no")
    return 0 if selected and safe == len(selected) else 3


if __name__ == "__main__":
    raise SystemExit(main())
