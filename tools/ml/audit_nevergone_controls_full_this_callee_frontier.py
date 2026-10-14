#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path

PLAN_FORMAT = "NEVERGONE.ControlsFullThisCalleeFrontierPlan/1"
EVIDENCE_FORMAT = "NEVERGONE.GhidraControlsFullThisCalleeParameterUses/1"
OUT_FORMAT = "NEVERGONE.ControlsFullThisCalleeFrontierAudit/1"
TARGET_FIELDS = {"0x12c", "0x130", "0x134"}
EXPECTED_EXTERNAL = 14
EXPECTED_DIRECT = 11
EXPECTED_INDIRECT = 3

def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))

def parse_int(value: str | None) -> int:
    try:
        return int(str(value))
    except (TypeError, ValueError):
        return -1

def main() -> int:
    ap = argparse.ArgumentParser(description="Audit first-hop external callee uses of unadjusted Controls*")
    ap.add_argument("plan_tsv", type=Path)
    ap.add_argument("evidence_tsv", type=Path)
    ap.add_argument("--details-output", required=True, type=Path)
    ap.add_argument("--require-complete", action="store_true")
    args = ap.parse_args()

    plan = read_tsv(args.plan_tsv)
    if len(plan) != EXPECTED_EXTERNAL or any(r.get("format") != PLAN_FORMAT for r in plan):
        raise ValueError("expected exact 14-row full-this external frontier plan")
    direct = [r for r in plan if r.get("site_kind") == "direct_external"]
    indirect = [r for r in plan if r.get("site_kind") == "indirect_external"]
    if len(direct) != EXPECTED_DIRECT or len(indirect) != EXPECTED_INDIRECT:
        raise ValueError("external direct/indirect split changed")
    if any(r.get("site_kind") not in {"direct_external", "indirect_external"} for r in plan):
        raise ValueError("unexpected site kind")

    evidence = read_tsv(args.evidence_tsv)
    if any(r.get("format") != EVIDENCE_FORMAT or r.get("row_type") not in {"summary", "use"} for r in evidence):
        raise ValueError("unexpected callee parameter-use evidence row")

    direct_keys = {(r["source_function_entry"], r["source_op_address"]) for r in direct}
    by_key: dict[tuple[str, str], list[dict[str, str]]] = defaultdict(list)
    for row in evidence:
        key = (row.get("source_function_entry", ""), row.get("source_op_address", ""))
        if key not in direct_keys:
            raise ValueError("evidence row outside direct frontier plan: " + ":".join(key))
        by_key[key].append(row)

    errors: list[str] = []
    use_counts: Counter[str] = Counter()
    target_writers: list[dict] = []
    frontier: list[dict] = []
    safe_direct_writes: list[dict] = []
    read_uses: list[dict] = []
    direct_summaries: list[dict] = []

    for planned in direct:
        key = (planned["source_function_entry"], planned["source_op_address"])
        rows = by_key.get(key, [])
        summaries = [r for r in rows if r["row_type"] == "summary"]
        uses = [r for r in rows if r["row_type"] == "use"]
        if len(summaries) != 1:
            errors.append(planned["source_op_address"] + ":summary_not_unique")
            continue
        summary = summaries[0]
        if summary.get("callee_entry") != planned.get("callee_entry"):
            errors.append(planned["source_op_address"] + ":callee_entry_mismatch")
        if summary.get("callee_parameter_index") != planned.get("callee_parameter_index"):
            errors.append(planned["source_op_address"] + ":callee_parameter_index_mismatch")
        if summary.get("decompile_status") != "ok":
            errors.append(planned["source_op_address"] + ":callee_decompile_failed")
        if summary.get("status") != "complete_parameter_use_export":
            errors.append(planned["source_op_address"] + ":" + summary.get("status", "missing_status"))
        if parse_int(summary.get("parameter_instances")) <= 0:
            errors.append(planned["source_op_address"] + ":parameter_instances_missing")

        actual = Counter(r.get("use_kind", "-") for r in uses)
        forward_expected = parse_int(summary.get("forwarded_call_count"))
        checks = {
            "direct_field_write": parse_int(summary.get("direct_write_count")),
            "retention_store": parse_int(summary.get("retention_store_count")),
            "return_escape": parse_int(summary.get("return_escape_count")),
            "dereference_read": parse_int(summary.get("dereference_read_count")),
            "opaque_use": parse_int(summary.get("opaque_use_count")),
        }
        for kind, expected in checks.items():
            if actual[kind] != expected:
                errors.append(planned["source_op_address"] + ":" + kind + "_count_mismatch")
        if actual["direct_forward_call"] + actual["indirect_forward_call"] != forward_expected:
            errors.append(planned["source_op_address"] + ":forward_count_mismatch")

        direct_summaries.append({
            "source_function_entry": planned["source_function_entry"],
            "source_op_address": planned["source_op_address"],
            "callee_category": planned["callee_category"],
            "callee_symbol": planned["callee_symbol"],
            "callee_entry": planned["callee_entry"],
            "callee_parameter_index": planned["callee_parameter_index"],
            "callee_name": summary.get("callee_name"),
            "use_counts": dict(sorted(actual.items())),
        })

        for use in uses:
            kind = use.get("use_kind", "-")
            use_counts[kind] += 1
            compact = {
                "source_function_entry": planned["source_function_entry"],
                "source_op_address": planned["source_op_address"],
                "callee_category": planned["callee_category"],
                "callee_symbol": planned["callee_symbol"],
                "callee_entry": planned["callee_entry"],
                "callee_name": summary.get("callee_name"),
                "callee_parameter_index": planned["callee_parameter_index"],
                "use_address": use.get("use_address"),
                "use_opcode": use.get("use_opcode"),
                "use_kind": kind,
                "value_offset": use.get("value_offset"),
                "target_entry": use.get("target_entry"),
                "target_name": use.get("target_name"),
                "target_argument_index": use.get("target_argument_index"),
                "destination_token": use.get("destination_token"),
                "status": use.get("status"),
            }
            if kind == "direct_field_write":
                if use.get("value_offset") in TARGET_FIELDS:
                    target_writers.append(compact)
                    frontier.append(compact)
                else:
                    safe_direct_writes.append(compact)
            elif kind == "dereference_read":
                read_uses.append(compact)
            else:
                frontier.append(compact)

    indirect_rows = [
        {
            "source_function_entry": r["source_function_entry"],
            "source_op_address": r["source_op_address"],
            "call_target_token": r["call_target_token"],
            "callee_category": r["callee_category"],
            "argument_index": r["argument_index"],
        }
        for r in indirect
    ]
    for item in indirect_rows:
        frontier.append({**item, "use_kind": "source_indirect_call"})

    status = "bounded_external_callee_frontier_complete" if not errors else "bounded_external_callee_frontier_incomplete"
    detail = {
        "format": OUT_FORMAT,
        "status": status,
        "external_site_count": len(plan),
        "direct_external_site_count": len(direct),
        "indirect_external_site_count": len(indirect),
        "direct_sites_complete": len(direct_summaries),
        "use_kind_counts": dict(sorted(use_counts.items())),
        "target_field_direct_writer_count": len(target_writers),
        "target_field_direct_writers": target_writers,
        "non_target_direct_write_count": len(safe_direct_writes),
        "dereference_read_count": len(read_uses),
        "frontier_count": len(frontier),
        "frontier": frontier,
        "indirect_source_sites": indirect_rows,
        "direct_site_summaries": direct_summaries,
        "reasons": sorted(set(errors)),
        "whole_program_alias_writes_excluded": False,
        "receiver_class_proven": False,
        "affects_routing": False,
    }
    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    args.details_output.write_text(json.dumps(detail, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", OUT_FORMAT)
    print("status:", status)
    print("external sites:", len(plan), "direct=", len(direct), "indirect=", len(indirect))
    print("direct sites complete:", len(direct_summaries), "/", len(direct))
    print("use kinds:", dict(sorted(use_counts.items())))
    print("target-field direct writers:", len(target_writers))
    print("next frontier:", len(frontier))
    print("reasons:", detail["reasons"])
    print("whole-program alias exclusion: no; affects routing: no")
    print("details:", args.details_output)
    return 2 if args.require_complete and errors else 0

if __name__ == "__main__":
    raise SystemExit(main())
