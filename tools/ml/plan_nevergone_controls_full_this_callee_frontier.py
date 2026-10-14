#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
import re
from collections import Counter
from pathlib import Path

TRIAGE_FORMAT = "NEVERGONE.ControlsFieldAliasTriage/1"
ELF_SHA = "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"
AUDIT_FORMAT = "NEVERGONE.ControlsThisCallArgumentOffsetAudit/1"
SOURCE_PLAN_FORMAT = "NEVERGONE.ControlsThisCallArgumentPlan/1"
OUT_FORMAT = "NEVERGONE.ControlsFullThisCalleeFrontierPlan/1"
EXPECTED_FULL_THIS = 35
EXPECTED_INTERNAL = 21
EXPECTED_EXTERNAL = 14
EXPECTED_DIRECT = 11
EXPECTED_INDIRECT = 3

def read_jsonl(path: Path) -> list[dict]:
    rows = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if not all(isinstance(row, dict) for row in rows):
        raise ValueError("expected JSON objects")
    return rows

def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))

def source_key(row: dict) -> tuple[str, str]:
    return str(row.get("function_entry")), str(row.get("op_address"))

def direct_entry(token: str) -> str | None:
    m = re.fullmatch(r"address:([0-9a-f]{8}):(?:4|8)", token or "")
    return m.group(1) if m else None

def main() -> int:
    ap = argparse.ArgumentParser(description="Plan external full-Controls-this callee parameter frontier")
    ap.add_argument("triage_jsonl", type=Path)
    ap.add_argument("offset_audit_jsonl", type=Path)
    ap.add_argument("source_plan_tsv", type=Path)
    ap.add_argument("--output", required=True, type=Path)
    args = ap.parse_args()

    triage = read_jsonl(args.triage_jsonl)
    if not triage or triage[0].get("format") != TRIAGE_FORMAT or triage[0].get("record_type") != "summary":
        raise ValueError("missing Controls alias triage summary")
    summary = triage[0]
    if (summary.get("elf_sha256") != ELF_SHA or summary.get("controls_methods_decompiled") != 38
            or summary.get("this_bearing_call_count") != 78):
        raise ValueError("stale or mismatched Controls alias triage")

    audits = read_jsonl(args.offset_audit_jsonl)
    if len(audits) != 1:
        raise ValueError("expected one Controls call-argument offset audit row")
    audit = audits[0]
    if audit.get("format") != AUDIT_FORMAT or audit.get("status") != "bounded_78_argument_offsets_complete":
        raise ValueError("offset audit format/status mismatch")
    if audit.get("full_controls_this_call_count") != EXPECTED_FULL_THIS:
        raise ValueError("full Controls this count changed")
    if audit.get("nonzero_controls_relative_call_count") != 43 or audit.get("unresolved_offset_call_count") != 0:
        raise ValueError("offset split changed")

    source = read_tsv(args.source_plan_tsv)
    if len(source) != 78 or any(row.get("format") != SOURCE_PLAN_FORMAT for row in source):
        raise ValueError("expected exact 78-row source plan")
    by_key = {source_key(row): row for row in source}
    if len(by_key) != 78:
        raise ValueError("duplicate source plan site")

    full = audit.get("full_controls_this_calls")
    if not isinstance(full, list) or len(full) != EXPECTED_FULL_THIS:
        raise ValueError("full Controls this site list mismatch")

    rows: list[dict[str, str]] = []
    internal = 0
    kind_counts: Counter[str] = Counter()
    category_counts: Counter[str] = Counter()

    for item in full:
        key = source_key(item)
        planned = by_key.get(key)
        if planned is None:
            raise ValueError(f"full-this site absent from source plan: {key}")
        for field in ("callee_category", "callee_symbol", "argument_index"):
            if str(item.get(field)) != str(planned.get(field)):
                raise ValueError(f"full-this/source-plan mismatch {key} field={field}")
        if item.get("address_offset") != "0x0":
            raise ValueError(f"non-zero site leaked into full-this list: {key}")

        category = planned["callee_category"]
        if category == "controls_method_candidate":
            internal += 1
            continue

        call_kind = planned["call_kind"]
        target = planned["call_target_token"]
        entry = direct_entry(target) if call_kind == "CALL" else None
        if call_kind == "CALL" and entry is not None:
            site_kind = "direct_external"
            callee_param = planned["argument_index"]
        elif call_kind == "CALLIND":
            site_kind = "indirect_external"
            entry = None
            callee_param = "-"
        else:
            site_kind = "unresolved_external"
            entry = None
            callee_param = "-"

        rows.append({
            "format": OUT_FORMAT,
            "source_function_entry": planned["function_entry"],
            "source_op_address": planned["op_address"],
            "source_op_time": planned["op_time"],
            "source_call_kind": call_kind,
            "call_target_token": target,
            "callee_category": category,
            "callee_symbol": planned["callee_symbol"],
            "argument_index": planned["argument_index"],
            "site_kind": site_kind,
            "callee_entry": entry or "-",
            "callee_parameter_index": callee_param,
        })
        kind_counts[site_kind] += 1
        category_counts[category] += 1

    if internal != EXPECTED_INTERNAL:
        raise ValueError(f"internal Controls edge count changed: {internal}")
    if len(rows) != EXPECTED_EXTERNAL:
        raise ValueError(f"external full-this site count changed: {len(rows)}")
    if kind_counts["direct_external"] != EXPECTED_DIRECT or kind_counts["indirect_external"] != EXPECTED_INDIRECT:
        raise ValueError(f"external site-kind split changed: {dict(kind_counts)}")
    if kind_counts.get("unresolved_external", 0):
        raise ValueError("unexpected unresolved direct external site")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    fields = [
        "format", "source_function_entry", "source_op_address", "source_op_time",
        "source_call_kind", "call_target_token", "callee_category", "callee_symbol",
        "argument_index", "site_kind", "callee_entry", "callee_parameter_index",
    ]
    with args.output.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, delimiter="\t", lineterminator="\n")
        w.writeheader()
        w.writerows(rows)

    print("format:", OUT_FORMAT)
    print("full Controls this sites:", EXPECTED_FULL_THIS)
    print("internal Controls edges:", internal)
    print("external frontier sites:", len(rows))
    print("site kinds:", dict(sorted(kind_counts.items())))
    print("external categories:", dict(sorted(category_counts.items())))
    print("output:", args.output)
    print("affects routing: no")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
