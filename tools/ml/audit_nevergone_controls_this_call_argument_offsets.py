#!/usr/bin/env python3
"""Audit exact Controls-relative offsets for all 78 parameter-zero-derived calls."""
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter, defaultdict
from pathlib import Path

PLAN_FORMAT = "NEVERGONE.ControlsThisCallArgumentPlan/1"
GHIDRA_FORMAT = "NEVERGONE.GhidraControlsThisCallArgumentOffsets/1"
OUT_FORMAT = "NEVERGONE.ControlsThisCallArgumentOffsetAudit/1"
EXPECTED_CATEGORIES = {
    "callback_factory_candidate": 1,
    "ccpoint_subobject_candidate": 43,
    "cocos_base_method_candidate": 6,
    "controls_method_candidate": 21,
    "controlsbutton_factory_callback_candidate": 3,
    "indirect_unresolved": 3,
    "operator_delete_candidate": 1,
}
EXPECTED_ARGUMENT_INDICES = {"0": 57, "1": 18, "2": 3}


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def site_key(row: dict[str, str], plan: bool) -> tuple[str, str, str]:
    return (
        row["function_entry"],
        row["op_address"] if plan else row["op_address"],
        row["op_time"],
    )


def parse_offset(value: str) -> int | None:
    if value == "-" or not value:
        return None
    try:
        if value.startswith("-0x"):
            return -int(value[3:], 16)
        if value.startswith("0x"):
            return int(value[2:], 16)
    except ValueError:
        return None
    return None


def audit(plan: list[dict[str, str]], evidence: list[dict[str, str]]) -> dict:
    if len(plan) != 78 or any(row.get("format") != PLAN_FORMAT for row in plan):
        raise ValueError("expected exact 78-row Controls call-argument plan")
    plan_keys = [site_key(row, True) for row in plan]
    if len(set(plan_keys)) != 78:
        raise ValueError("duplicate planned call site")
    if Counter(row.get("callee_category") for row in plan) != Counter(EXPECTED_CATEGORIES):
        raise ValueError("planned callee categories changed")
    if Counter(row.get("argument_index") for row in plan) != Counter(EXPECTED_ARGUMENT_INDICES):
        raise ValueError("planned argument-index distribution changed")

    if any(
        row.get("format") != GHIDRA_FORMAT
        or row.get("row_type") not in {"summary", "argument"}
        for row in evidence
    ):
        raise ValueError("unexpected Ghidra call-argument offset row")

    by_site: dict[tuple[str, str, str], list[dict[str, str]]] = defaultdict(list)
    valid_keys = set(plan_keys)
    for row in evidence:
        key = site_key(row, False)
        if key not in valid_keys:
            raise ValueError("Ghidra evidence row outside pinned plan: " + ":".join(key))
        by_site[key].append(row)

    errors: list[str] = []
    details: list[dict] = []
    category_offsets: dict[str, Counter[str]] = defaultdict(Counter)
    kind_offsets: dict[str, Counter[str]] = defaultdict(Counter)
    full_this_calls: list[dict] = []
    nonzero_calls: list[dict] = []
    unresolved_calls: list[dict] = []

    for planned in plan:
        key = site_key(planned, True)
        rows = by_site.get(key, [])
        summaries = [row for row in rows if row["row_type"] == "summary"]
        arguments = [row for row in rows if row["row_type"] == "argument"]
        reasons: list[str] = []

        if len(summaries) != 1:
            reasons.append("summary_not_unique")
        else:
            summary = summaries[0]
            if summary.get("status") != "ok":
                reasons.append("site_" + summary.get("status", "missing_status"))
            if summary.get("planned_target_token") != planned["call_target_token"]:
                reasons.append("planned_target_mismatch")
            if summary.get("observed_target_token") != planned["call_target_token"]:
                reasons.append("observed_target_mismatch")
            if summary.get("call_kind") != planned["call_kind"]:
                reasons.append("call_kind_mismatch")
            if summary.get("callee_category") != planned["callee_category"]:
                reasons.append("callee_category_mismatch")
            if summary.get("callee_symbol") != planned["callee_symbol"]:
                reasons.append("callee_symbol_mismatch")
            if summary.get("argument_index") != planned["argument_index"]:
                reasons.append("argument_index_mismatch")
            try:
                selected = int(summary.get("selected_argument_count", "-1"))
            except ValueError:
                selected = -1
            if selected != 1:
                reasons.append("selected_argument_count_not_one")

        if len(arguments) != 1:
            reasons.append("argument_not_unique")
            argument = None
        else:
            argument = arguments[0]
            if argument.get("status") != "ok":
                reasons.append("argument_status_not_ok")
            if argument.get("argument_index") != planned["argument_index"]:
                reasons.append("argument_index_mismatch")
            if argument.get("root_parameter_index") != "0":
                reasons.append("parameter_zero_provenance_lost")
            offset = parse_offset(argument.get("address_offset", "-"))
            if offset is None:
                reasons.append("argument_offset_unresolved")

        errors.extend(f'{planned["op_address"]}:{reason}' for reason in reasons)
        detail = {
            "function_entry": planned["function_entry"],
            "op_address": planned["op_address"],
            "op_time": planned["op_time"],
            "call_kind": planned["call_kind"],
            "callee_category": planned["callee_category"],
            "callee_symbol": planned["callee_symbol"],
            "argument_index": planned["argument_index"],
            "status": "ok" if not reasons else "incomplete",
            "reasons": reasons,
            "affects_routing": False,
        }
        if argument is not None:
            detail.update(
                {
                    "operand_token": argument.get("operand_token"),
                    "address_kind": argument.get("address_kind"),
                    "address_root_token": argument.get("address_root_token"),
                    "address_offset": argument.get("address_offset"),
                    "root_parameter_index": argument.get("root_parameter_index"),
                }
            )
            offset = parse_offset(argument.get("address_offset", "-"))
            category_offsets[planned["callee_category"]][argument.get("address_offset", "-")] += 1
            kind_offsets[argument.get("address_kind", "-")][argument.get("address_offset", "-")] += 1
            compact = {
                "function_entry": planned["function_entry"],
                "op_address": planned["op_address"],
                "callee_category": planned["callee_category"],
                "callee_symbol": planned["callee_symbol"],
                "argument_index": planned["argument_index"],
                "address_offset": argument.get("address_offset"),
            }
            if offset is None:
                unresolved_calls.append(compact)
            elif offset == 0:
                full_this_calls.append(compact)
            else:
                nonzero_calls.append(compact)
        details.append(detail)

    status = "bounded_78_argument_offsets_complete" if not errors else "bounded_78_argument_offsets_incomplete"
    return {
        "format": OUT_FORMAT,
        "status": status,
        "sites_expected": 78,
        "sites_complete": sum(item["status"] == "ok" for item in details),
        "category_counts": dict(sorted(Counter(row["callee_category"] for row in plan).items())),
        "argument_index_counts": dict(sorted(Counter(row["argument_index"] for row in plan).items())),
        "category_offsets": {
            category: dict(sorted(counter.items()))
            for category, counter in sorted(category_offsets.items())
        },
        "address_kind_offsets": {
            kind: dict(sorted(counter.items()))
            for kind, counter in sorted(kind_offsets.items())
        },
        "full_controls_this_call_count": len(full_this_calls),
        "nonzero_controls_relative_call_count": len(nonzero_calls),
        "unresolved_offset_call_count": len(unresolved_calls),
        "full_controls_this_calls": full_this_calls,
        "nonzero_controls_relative_calls": nonzero_calls,
        "unresolved_offset_calls": unresolved_calls,
        "sites": details,
        "reasons": sorted(set(errors)),
        "whole_program_alias_writes_excluded": False,
        "receiver_class_proven": False,
        "affects_routing": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("plan_tsv", type=Path)
    parser.add_argument("ghidra_argument_offsets_tsv", type=Path)
    parser.add_argument("--details-output", required=True, type=Path)
    parser.add_argument("--require-complete", action="store_true")
    args = parser.parse_args()

    report = audit(read_tsv(args.plan_tsv), read_tsv(args.ghidra_argument_offsets_tsv))
    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    args.details_output.write_text(json.dumps(report, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", OUT_FORMAT)
    print("status:", report["status"])
    print("sites:", report["sites_complete"], "/", report["sites_expected"])
    print("full Controls this:", report["full_controls_this_call_count"])
    print("nonzero Controls-relative:", report["nonzero_controls_relative_call_count"])
    print("unresolved offset:", report["unresolved_offset_call_count"])
    print("category offsets:", report["category_offsets"])
    print("whole-program alias exclusion: no; affects routing: no")
    print("details:", args.details_output)
    return 2 if args.require_complete and report["reasons"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
