#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

CONSUMER_FORMAT = "NEVERGONE.GhidraAccessorReturnConsumers/1"
ACCESSOR_AUDIT_FORMAT = "NEVERGONE.GameSceneUIControlsAccessorAudit/1"
CATALOG_FORMAT = "NEVERGONE.StaticVtableSlotCatalog/3"
OUT_FORMAT = "NEVERGONE.GameSceneUIControlsReturnConsumerAudit/1"


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line_no, raw in enumerate(handle, 1):
            if not raw.strip():
                continue
            value = json.loads(raw)
            if not isinstance(value, dict):
                raise ValueError(f"{path}:{line_no}: expected object")
            rows.append(value)
    return rows


def parse_int(text: str | None) -> int:
    try:
        return int(text or "0", 10)
    except ValueError:
        return 0


def mapped_class(target_name: str, classes: set[str]) -> str | None:
    matches = [cls for cls in classes if target_name.startswith(cls + "::")]
    if not matches:
        return None
    matches.sort(key=len, reverse=True)
    if len(matches) >= 2 and len(matches[0]) == len(matches[1]):
        return None
    return matches[0]


def main() -> int:
    parser = argparse.ArgumentParser(description="Audit direct member consumers of GameSceneUI::GetControlsObj return value")
    parser.add_argument("consumer_tsv", type=Path)
    parser.add_argument("accessor_audit_jsonl", type=Path)
    parser.add_argument("catalog_jsonl", type=Path)
    parser.add_argument("--details-output", type=Path)
    args = parser.parse_args()

    consumers = read_tsv(args.consumer_tsv)
    accessor_audit = read_jsonl(args.accessor_audit_jsonl)
    catalog_rows = read_jsonl(args.catalog_jsonl)

    for line_no, row in enumerate(consumers, 2):
        if row.get("format") != CONSUMER_FORMAT:
            raise ValueError(f"{args.consumer_tsv}:{line_no}: expected {CONSUMER_FORMAT}")
    if len(accessor_audit) != 1 or accessor_audit[0].get("format") != ACCESSOR_AUDIT_FORMAT:
        raise ValueError(f"{args.accessor_audit_jsonl}: expected one {ACCESSOR_AUDIT_FORMAT} row")

    class_vtables: dict[str, set[str]] = defaultdict(set)
    for line_no, row in enumerate(catalog_rows, 1):
        if row.get("format") != CATALOG_FORMAT:
            raise ValueError(f"{args.catalog_jsonl}:{line_no}: expected {CATALOG_FORMAT}")
        cls = row.get("class_name")
        vt = row.get("receiver_vtable_id")
        if isinstance(cls, str) and cls and isinstance(vt, str) and vt:
            class_vtables[cls].add(vt)
    classes = set(class_vtables)

    summaries = [r for r in consumers if r.get("row_type") == "summary"]
    uses = [r for r in consumers if r.get("row_type") == "use"]
    direct_this = [r for r in uses if r.get("status") == "direct_this_call"]
    target_names = sorted({r.get("direct_target_name") or "" for r in direct_this if r.get("direct_target_name") not in {None, "", "-"}})

    mapped_rows: list[dict[str, Any]] = []
    mapped_classes: set[str] = set()
    unmapped_targets: set[str] = set()
    for row in direct_this:
        target = row.get("direct_target_name") or ""
        cls = mapped_class(target, classes)
        if cls is None:
            if target and target != "-":
                unmapped_targets.add(target)
            continue
        mapped_classes.add(cls)
        mapped_rows.append({
            "caller_entry": row.get("caller_entry"),
            "caller_name": row.get("caller_name"),
            "accessor_call_site": row.get("accessor_call_site"),
            "use_address": row.get("use_address"),
            "target_name": target,
            "mapped_class": cls,
        })

    accessor = accessor_audit[0]
    chain_ok = (
        accessor.get("field_return_rows") == 1
        and accessor.get("accessor_offsets") == ["0x134"]
        and accessor.get("mixed_direct_singleton_value_present") is True
    )

    reasons: list[str] = []
    if len(summaries) != 1:
        reasons.append("consumer_summary_not_unique")
    elif summaries[0].get("status") != "complete_return_use_export":
        reasons.append("consumer_export_incomplete")
    if not chain_ok:
        reasons.append("accessor_0x134_chain_not_proven")
    if not direct_this:
        reasons.append("no_direct_this_consumers")
    if not mapped_classes:
        reasons.append("no_catalog_member_consumer_class")

    if reasons:
        status = "consumer_evidence_incomplete"
    elif len(mapped_classes) == 1 and not unmapped_targets:
        status = "unique_catalog_member_consumer_class"
    elif len(mapped_classes) == 1:
        status = "unique_mapped_class_with_unmapped_consumers"
    else:
        status = "multiple_catalog_member_consumer_classes"

    vtables = sorted({vt for cls in mapped_classes for vt in class_vtables.get(cls, set())})
    summary = summaries[0] if len(summaries) == 1 else {}
    details = {
        "format": OUT_FORMAT,
        "accessor_chain_ok": chain_ok,
        "direct_caller_functions": parse_int(summary.get("direct_caller_functions")),
        "caller_functions_decompiled": parse_int(summary.get("caller_functions_decompiled")),
        "caller_functions_failed": parse_int(summary.get("caller_functions_failed")),
        "accessor_call_sites": parse_int(summary.get("accessor_call_sites")),
        "accessor_calls_with_output": parse_int(summary.get("accessor_calls_with_output")),
        "direct_member_uses": len(direct_this),
        "unsupported_uses": parse_int(summary.get("unsupported_uses")),
        "target_names": target_names,
        "mapped_classes": sorted(mapped_classes),
        "mapped_vtables": vtables,
        "unmapped_targets": sorted(unmapped_targets),
        "mapped_rows": mapped_rows,
        "status": status,
        "reasons": reasons,
        "affects_routing": False,
    }

    if args.details_output is not None:
        args.details_output.parent.mkdir(parents=True, exist_ok=True)
        args.details_output.write_text(json.dumps(details, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", OUT_FORMAT)
    print("accessor +0x134 chain proven:", chain_ok)
    print("direct caller functions:", details["direct_caller_functions"])
    print("caller functions decompiled:", details["caller_functions_decompiled"])
    print("caller functions failed:", details["caller_functions_failed"])
    print("accessor call sites:", details["accessor_call_sites"])
    print("accessor calls with output:", details["accessor_calls_with_output"])
    print("direct member uses:", len(direct_this))
    print("unsupported uses:", details["unsupported_uses"])
    print("mapped classes:", sorted(mapped_classes))
    print("mapped vtables:", vtables)
    print("unmapped targets:", sorted(unmapped_targets))
    print("status:", status)
    print("reasons:", reasons)
    print("member targets:")
    counts = Counter(target_names)
    for target in target_names:
        print(" ", target, "class=", mapped_class(target, classes), "count=", counts[target])
    if args.details_output is not None:
        print("details output:", args.details_output)
    print("affects routing: no")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
