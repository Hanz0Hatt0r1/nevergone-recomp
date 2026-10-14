#!/usr/bin/env python3
"""Synthetic regression for the bounded 78-site Controls call-argument offset audit."""
from __future__ import annotations

import copy

from audit_nevergone_controls_this_call_argument_offsets import (
    EXPECTED_ARGUMENT_INDICES,
    EXPECTED_CATEGORIES,
    GHIDRA_FORMAT,
    PLAN_FORMAT,
    audit,
)


def fixtures():
    categories = []
    for category, count in EXPECTED_CATEGORIES.items():
        categories.extend([category] * count)
    indices = []
    for index, count in EXPECTED_ARGUMENT_INDICES.items():
        indices.extend([index] * count)
    assert len(categories) == len(indices) == 78

    plan = []
    evidence = []
    for i, (category, argument_index) in enumerate(zip(categories, indices)):
        address = f"{0x2A1000 + 4 * i:08x}"
        target = f"address:{0x500000 + 4 * i:08x}:8"
        symbol = f"symbol_{i}"
        planned = {
            "format": PLAN_FORMAT,
            "function_entry": "002a7e2c",
            "op_address": address,
            "op_time": str(i),
            "call_kind": "CALL",
            "call_target_token": target,
            "callee_category": category,
            "callee_symbol": symbol,
            "argument_index": argument_index,
        }
        plan.append(planned)
        offset = "0x0" if i % 3 else "0x168"
        evidence.append(
            {
                "format": GHIDRA_FORMAT,
                "row_type": "summary",
                "function_entry": planned["function_entry"],
                "op_address": address,
                "op_time": str(i),
                "call_kind": "CALL",
                "planned_target_token": target,
                "observed_target_token": target,
                "callee_category": category,
                "callee_symbol": symbol,
                "argument_index": argument_index,
                "operand_index": "-",
                "operand_token": "-",
                "address_kind": "-",
                "address_root_token": "-",
                "address_offset": "-",
                "root_parameter_index": "-",
                "status": "ok",
                "selected_argument_count": "1",
            }
        )
        evidence.append(
            {
                **evidence[-1],
                "row_type": "argument",
                "operand_index": str(int(argument_index) + 1),
                "operand_token": "register:register:00000020:4",
                "address_kind": "base_plus_constant" if offset != "0x0" else "base",
                "address_root_token": "register:register:00000020:4",
                "address_offset": offset,
                "root_parameter_index": "0",
                "selected_argument_count": "-",
            }
        )
    return plan, evidence


def expect_incomplete(plan, evidence, text):
    report = audit(plan, evidence)
    assert report["status"] == "bounded_78_argument_offsets_incomplete", report
    assert any(text in reason for reason in report["reasons"]), report


def main() -> int:
    plan, evidence = fixtures()
    report = audit(plan, evidence)
    assert report["status"] == "bounded_78_argument_offsets_complete"
    assert report["sites_complete"] == 78
    assert report["full_controls_this_call_count"] == 52
    assert report["nonzero_controls_relative_call_count"] == 26
    assert report["unresolved_offset_call_count"] == 0
    assert report["whole_program_alias_writes_excluded"] is False
    assert report["receiver_class_proven"] is False
    assert report["affects_routing"] is False

    bad = copy.deepcopy(evidence)
    bad.pop(0)
    expect_incomplete(plan, bad, "summary_not_unique")

    bad = copy.deepcopy(evidence)
    bad[1]["root_parameter_index"] = "-"
    expect_incomplete(plan, bad, "parameter_zero_provenance_lost")

    bad = copy.deepcopy(evidence)
    bad[1]["address_offset"] = "-"
    expect_incomplete(plan, bad, "argument_offset_unresolved")

    bad = copy.deepcopy(evidence)
    bad[0]["observed_target_token"] = "address:deadbeef:8"
    expect_incomplete(plan, bad, "observed_target_mismatch")

    bad = copy.deepcopy(evidence)
    bad[0]["selected_argument_count"] = "0"
    expect_incomplete(plan, bad, "selected_argument_count_not_one")

    bad = copy.deepcopy(evidence)
    bad[-1]["op_address"] = "deadbeef"
    try:
        audit(plan, bad)
    except ValueError:
        pass
    else:
        raise AssertionError("evidence outside pinned plan accepted")

    try:
        audit(plan[:-1], evidence)
    except ValueError:
        pass
    else:
        raise AssertionError("incomplete 78-site plan accepted")

    print("Controls this-call argument offset audit smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
