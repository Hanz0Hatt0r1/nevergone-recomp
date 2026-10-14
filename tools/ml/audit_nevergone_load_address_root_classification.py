#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
from pathlib import Path
from typing import Any

PLAN_FORMAT = "NEVERGONE.LoadAddressRootProbePlan/1"
CLASS_FORMAT = "NEVERGONE.GhidraAddressClassification/1"
OUT_FORMAT = "NEVERGONE.LoadAddressRootClassificationAudit/1"


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def addr_key(value: Any) -> str | None:
    if value in (None, "", "-"):
        return None
    text = str(value).strip()
    try:
        number = int(text, 0)
    except ValueError:
        try:
            number = int(text, 16)
        except ValueError:
            return None
    return f"{number:08x}"


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit evidence-only classification of LOAD address roots and their raw words")
    parser.add_argument("plan_tsv", type=Path)
    parser.add_argument("root_classification_tsv", type=Path)
    parser.add_argument("target_classification_tsv", type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    plan = read_tsv(args.plan_tsv)
    roots = read_tsv(args.root_classification_tsv)
    targets = read_tsv(args.target_classification_tsv)

    for line_no, row in enumerate(plan, 2):
        if row.get("format") != PLAN_FORMAT:
            raise ValueError(f"{args.plan_tsv}:{line_no}: expected {PLAN_FORMAT}")
    for path, rows in ((args.root_classification_tsv, roots), (args.target_classification_tsv, targets)):
        for line_no, row in enumerate(rows, 2):
            if row.get("format") != CLASS_FORMAT:
                raise ValueError(f"{path}:{line_no}: expected {CLASS_FORMAT}")

    root_by_address = {addr_key(row.get("address")): row for row in roots}
    target_by_address = {addr_key(row.get("address")): row for row in targets}
    details: list[dict[str, Any]] = []
    buckets = Counter()

    for item in plan:
        root_key = addr_key(item.get("root_address"))
        root = root_by_address.get(root_key)
        reasons: list[str] = []
        if root is None:
            reasons.append("root_classification_missing")
            raw_key = None
            target = None
            status = "classification_incomplete"
        else:
            raw_key = addr_key(root.get("raw_u32"))
            target = target_by_address.get(raw_key) if raw_key is not None else None
            if raw_key is None:
                reasons.append("root_raw_u32_missing")
            elif target is None:
                reasons.append("raw_target_classification_missing")

            instruction = root.get("instruction_mnemonic") not in {None, "", "-"}
            root_block = root.get("block") or "-"
            root_exec = root.get("execute") == "1"
            target_mapped = target is not None and target.get("block") not in {None, "", "-"}
            target_symbolic = target is not None and (
                target.get("primary_symbol") not in {None, "", "-"}
                or target.get("function_at_name") not in {None, "", "-"}
                or target.get("function_containing_name") not in {None, "", "-"}
            )

            if reasons:
                status = "classification_incomplete"
            elif instruction:
                status = "root_is_instruction_not_literal"
            elif root_exec and target_mapped:
                status = "executable_block_literal_points_to_mapped_address"
            elif target_mapped and target_symbolic:
                status = "raw_word_points_to_symbolic_mapped_address"
            elif target_mapped:
                status = "raw_word_points_to_mapped_address"
            else:
                status = "raw_word_unmapped"

        buckets[status] += 1
        details.append({
            "format": OUT_FORMAT,
            "source_id": item.get("source_id"),
            "call_index": int(item.get("call_index", "-1")),
            "call_address": item.get("call_address"),
            "dispatch_slot_offset": item.get("dispatch_slot_offset"),
            "root_address": item.get("root_address"),
            "root_block": root.get("block") if root else None,
            "root_execute": root.get("execute") if root else None,
            "root_instruction": root.get("instruction_mnemonic") if root else None,
            "root_data_type": root.get("data_type") if root else None,
            "root_raw_u32": root.get("raw_u32") if root else None,
            "root_containing_function": root.get("function_containing_name") if root else None,
            "raw_target_address": raw_key,
            "raw_target_block": target.get("block") if target else None,
            "raw_target_symbol": target.get("primary_symbol") if target else None,
            "raw_target_function": target.get("function_at_name") if target else None,
            "raw_target_containing_function": target.get("function_containing_name") if target else None,
            "raw_target_data_type": target.get("data_type") if target else None,
            "raw_target_raw_u32": target.get("raw_u32") if target else None,
            "status": status,
            "reasons": reasons,
            "affects_routing": False,
        })

    print("== LOAD address-root classification audit ==")
    print("format:", OUT_FORMAT)
    print("planned roots:", len(plan))
    print("bucket counts:", dict(buckets))
    print("rows:")
    for row in details:
        print(
            row["source_id"], "call", row["call_index"], "slot=", row["dispatch_slot_offset"],
            "root=", row["root_address"], "block=", row["root_block"],
            "instruction=", row["root_instruction"], "raw=", row["root_raw_u32"],
            "target=", row["raw_target_address"], "target_block=", row["raw_target_block"],
            "target_symbol=", row["raw_target_symbol"], "target_function=", row["raw_target_function"],
            "target_raw=", row["raw_target_raw_u32"], "status=", row["status"],
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
