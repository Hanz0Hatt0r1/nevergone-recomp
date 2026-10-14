#!/usr/bin/env python3
"""Summarize folded receiver LOAD addresses, raw values, and direct xref owners.

Evidence-only. It does not infer receiver classes or emit routing queries.
"""
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

FOLD_FORMAT = "NEVERGONE.LoadAddressExpressionFoldAudit/1"
CLASS_FORMAT = "NEVERGONE.GhidraAddressClassification/1"
REF_FORMAT = "NEVERGONE.GhidraAddressReferenceOwners/1"
OUT_FORMAT = "NEVERGONE.LoadEffectiveAddressOwnerAudit/1"


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


def address_key(value: Any) -> str | None:
    if value in (None, "", "-"):
        return None
    if isinstance(value, int):
        return f"{value & 0xffffffff:08x}"
    text = str(value).strip().lower()
    try:
        if text.startswith("0x"):
            number = int(text, 16)
        else:
            number = int(text, 16)
    except ValueError:
        return None
    return f"{number & 0xffffffff:08x}"


def unique(values: list[str]) -> list[str]:
    return sorted({value for value in values if value not in {"", "-", None}})


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fold_jsonl", type=Path)
    parser.add_argument("effective_classification_tsv", type=Path)
    parser.add_argument("value_classification_tsv", type=Path)
    parser.add_argument("reference_owners_tsv", type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    folds = read_jsonl(args.fold_jsonl)
    effective_rows = read_tsv(args.effective_classification_tsv)
    value_rows = read_tsv(args.value_classification_tsv)
    ref_rows = read_tsv(args.reference_owners_tsv)

    for line_no, row in enumerate(folds, 1):
        if row.get("format") != FOLD_FORMAT:
            raise ValueError(f"{args.fold_jsonl}:{line_no}: expected {FOLD_FORMAT}")
    for path, rows, fmt in (
        (args.effective_classification_tsv, effective_rows, CLASS_FORMAT),
        (args.value_classification_tsv, value_rows, CLASS_FORMAT),
        (args.reference_owners_tsv, ref_rows, REF_FORMAT),
    ):
        for line_no, row in enumerate(rows, 2):
            if row.get("format") != fmt:
                raise ValueError(f"{path}:{line_no}: expected {fmt}")

    effective_by = {address_key(row.get("address")): row for row in effective_rows}
    value_by = {address_key(row.get("address")): row for row in value_rows}
    refs_by: dict[str, list[dict[str, str]]] = defaultdict(list)
    for row in ref_rows:
        key = address_key(row.get("target_address"))
        if key is not None:
            refs_by[key].append(row)

    details: list[dict[str, Any]] = []
    buckets = Counter()

    for fold in folds:
        effective_key = address_key(fold.get("effective_address"))
        effective = effective_by.get(effective_key) if effective_key else None
        raw_key = address_key(effective.get("raw_u32")) if effective else None
        value = value_by.get(raw_key) if raw_key else None
        refs = refs_by.get(effective_key, []) if effective_key else []
        real_refs = [row for row in refs if row.get("reference_from") not in {None, "", "-"}]

        reasons: list[str] = []
        if fold.get("status") != "effective_address_folded" or effective_key is None:
            status = "fold_incomplete"
            reasons.append(str(fold.get("status")))
        elif effective is None:
            status = "effective_classification_missing"
            reasons.append("effective_classification_missing")
        elif effective.get("block") in {None, "", "-"}:
            status = "effective_address_unmapped"
        elif effective.get("instruction_mnemonic") not in {None, "", "-"}:
            status = "effective_address_is_instruction"
        elif effective.get("write") == "1" and real_refs:
            status = "writable_effective_cell_with_refs"
        elif effective.get("write") == "1":
            status = "writable_effective_cell_no_refs"
        elif real_refs:
            status = "mapped_effective_data_with_refs"
        else:
            status = "mapped_effective_data_no_refs"
        buckets[status] += 1

        detail = {
            "format": OUT_FORMAT,
            "source_id": fold.get("source_id"),
            "call_index": fold.get("call_index"),
            "call_address": fold.get("call_address"),
            "dispatch_slot_offset": fold.get("dispatch_slot_offset"),
            "effective_address": effective_key,
            "effective_block": effective.get("block") if effective else None,
            "effective_read": effective.get("read") if effective else None,
            "effective_write": effective.get("write") if effective else None,
            "effective_execute": effective.get("execute") if effective else None,
            "effective_symbol": effective.get("primary_symbol") if effective else None,
            "effective_instruction": effective.get("instruction_mnemonic") if effective else None,
            "effective_data_type": effective.get("data_type") if effective else None,
            "effective_raw_u32": effective.get("raw_u32") if effective else None,
            "raw_value_address": raw_key,
            "raw_value_block": value.get("block") if value else None,
            "raw_value_symbol": value.get("primary_symbol") if value else None,
            "raw_value_function": value.get("function_at_name") if value else None,
            "raw_value_containing_function": value.get("function_containing_name") if value else None,
            "raw_value_instruction": value.get("instruction_mnemonic") if value else None,
            "raw_value_data_type": value.get("data_type") if value else None,
            "reference_count": len(real_refs),
            "reference_types": unique([row.get("reference_type", "-") for row in real_refs]),
            "reference_sources": unique([row.get("reference_source", "-") for row in real_refs]),
            "owner_functions": unique([row.get("containing_function_name", "-") for row in real_refs]),
            "owner_entries": unique([row.get("containing_function_entry", "-") for row in real_refs]),
            "from_symbols": unique([row.get("from_symbol", "-") for row in real_refs]),
            "nearby_class_evidence": unique([row.get("nearby_class_evidence", "-") for row in real_refs]),
            "status": status,
            "reasons": reasons,
            "affects_routing": False,
        }
        details.append(detail)

    print("format:", OUT_FORMAT)
    print("fold rows:", len(folds))
    print("bucket counts:", dict(buckets))
    print("rows:")
    for row in details:
        print(
            row["source_id"], "call", row["call_index"], "slot=", row["dispatch_slot_offset"],
            "effective=", row["effective_address"], "block=", row["effective_block"],
            "rwxe=", f"{row['effective_read']}{row['effective_write']}{row['effective_execute']}",
            "symbol=", row["effective_symbol"], "data=", row["effective_data_type"],
            "raw=", row["effective_raw_u32"], "raw_target=", row["raw_value_address"],
            "raw_block=", row["raw_value_block"], "raw_symbol=", row["raw_value_symbol"],
            "raw_function=", row["raw_value_function"], "refs=", row["reference_count"],
            "owners=", row["owner_functions"], "status=", row["status"],
        )
        if row["nearby_class_evidence"]:
            print("  nearby class evidence:", row["nearby_class_evidence"])

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
