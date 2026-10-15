#!/usr/bin/env python3
"""Partition BeatenEffect whole-owner calls using vtable and p-code evidence.

The six indirect init calls have a unique static CCNode::addChild candidate. Callback
captures and lifecycle calls remain alias barriers; this audit promotes no
receiver classes.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

from audit_nevergone_beaten_factory_fields_v26 import rows

FORMAT = "NEVERGONE.BeatenOwnerCallAuditV28/1"
OWNER = "register:register:00000020:4"
ADD_CHILD = "libcocos2dcpp.so:00525dac"
INDIRECT = {"002a405e", "002a41d6", "002a42d0", "002a4478", "002a45a6", "002a469a"}
CALLBACKS = {"002a3f82", "002a40e2"}
LIFECYCLE = {"002a39ba", "002a39c0", "002a39dc", "002a39e2", "002a3e20"}


def owner_calls(flow: list[dict[str, str]]) -> dict[str, dict[str, str]]:
    calls = {}
    for row in flow:
        if row["row_type"] != "call_arg" or row["arg_flow_kind"] != "owner_address" or row["arg_owner_offset"] != "0x0":
            continue
        if row["arg_index"] != "0":
            raise ValueError(f"owner passed at unexpected argument: {row['op_address']}")
        address = row["op_address"]
        if address in calls:
            raise ValueError(f"duplicate whole-owner call: {address}")
        calls[address] = row
    expected = INDIRECT | CALLBACKS | LIFECYCLE
    if set(calls) != expected:
        raise ValueError(f"whole-owner call frontier changed: missing={expected-set(calls)}, extra={set(calls)-expected}")
    return calls


def catalog_target(catalog: Path) -> dict[str, object]:
    matched = []
    with catalog.open(encoding="utf-8") as stream:
        for line in stream:
            if not line.strip():
                continue
            row = json.loads(line)
            if row.get("class_name") == "BeatenEffect" and row.get("dispatch_slot_offset") == "0xbc":
                matched.append(row)
    if len(matched) != 1:
        raise ValueError(f"expected one BeatenEffect slot 0xbc catalog row; found {len(matched)}")
    row = matched[0]
    if (row.get("receiver_vtable_id") != "libcocos2dcpp.so:008f3fa8"
            or row.get("candidate_target_ids") != [ADD_CHILD]
            or row.get("candidate_target_count") != 1
            or row.get("static_status") != "single_function_memory_pointer"
            or row.get("runtime_validation_status") != "runtime_unobserved"
            or [t.get("name") for t in row.get("target_functions", [])] != ["cocos2d::CCNode::addChild"]):
        raise ValueError("BeatenEffect slot 0xbc vtable target changed")
    return {"receiver_vtable_id": row["receiver_vtable_id"], "dispatch_slot_offset": "0xbc",
            "runtime_validation_status": row["runtime_validation_status"],
            "target_id": ADD_CHILD, "target_name": "cocos2d::CCNode::addChild"}


def audit(flow_path: Path, slice_path: Path, catalog_path: Path, pcode_path: Path) -> dict[str, object]:
    calls = owner_calls(rows(flow_path))
    target = catalog_target(catalog_path)
    summaries = {r["call_address"]: r for r in rows(slice_path) if r["row_type"] == "summary"}
    resolved = []
    for address in sorted(INDIRECT):
        call = calls[address]
        slice_row = summaries.get(address)
        if call["call_kind"] != "CALLIND" or slice_row is None:
            raise ValueError(f"missing structural indirect call evidence: {address}")
        if (slice_row["recovery_status"] != "canonical_receiver"
                or slice_row["structural_receiver_token"] != OWNER
                or slice_row["slot_offset"] != "0xbc"
                or any(slice_row[k] != "0" for k in ("truncated_by_budget", "depth_truncation_count", "cycle_count"))):
            raise ValueError(f"unsafe or changed indirect receiver slice: {address}")
        resolved.append({"call_address": address, "call_index": int(slice_row["call_index"]), **target})
    pcode = [r for r in rows(pcode_path) if r["opcode"] == "CALL"]
    callback_calls = []
    for address in sorted(CALLBACKS):
        call = calls[address]
        match = [r for r in pcode if r["op_address"] == address]
        if (call["call_kind"] != "CALL" or call["call_target_name"] != "cocos2d::CCCallFunc::create"
                or len(match) != 1 or match[0]["callee_entry"] != "0051f3c8"
                or match[0]["callee_name"] != "cocos2d::CCCallFunc::create"):
            raise ValueError(f"callback capture evidence changed: {address}")
        callback_calls.append({"call_address": address, "callee": "cocos2d::CCCallFunc::create",
                               "owner_argument": 0, "potential_escape": True, "alias_barrier": True})
    lifecycle_calls = [{"call_address": address, "callee": calls[address]["call_target_name"]}
                       for address in sorted(LIFECYCLE)]
    return {"format": FORMAT, "catalog_matched_indirect_owner_calls": resolved,
            "callback_owner_passes": callback_calls, "lifecycle_owner_calls": lifecycle_calls,
            "whole_owner_calls_checked": len(calls), "alias_frontier_closed": False,
            "receiver_promotions": 0, "affects_routing": False}


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("owner_flow", type=Path)
    parser.add_argument("structural_slice", type=Path)
    parser.add_argument("vtable_catalog", type=Path)
    parser.add_argument("init_pcode", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.owner_flow, args.structural_slice, args.vtable_catalog, args.init_pcode)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"whole-owner calls: {result['whole_owner_calls_checked']}; addChild static candidates: 6; callback alias barriers: 2")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
