#!/usr/bin/env python3
"""Fail-closed synthetic tests; original proprietary binary is never committed."""
from __future__ import annotations

import copy
from audit_nevergone_controlsbutton_field_receiver_witness import (
    CALLS, FACTORY_STORES, SOURCE, verify_slices, require,
)


def fake_evidence():
    inference, rows = [], []
    for index, (site, load, field) in CALLS.items():
        address = f"{site + 0x10000:08x}"
        inference.append({
            "source_id": SOURCE, "call_index": index, "call_address": address,
            "dispatch_slot_offset": "0x80",
            "status": "receiver_not_single_pointer" if index in (3, 4, 7)
                      else "unresolved_receiver_class",
        })
        common = {
            "format": "NEVERGONE.GhidraStructuralReceiverSlice/1",
            "function_entry": "002a7594", "call_index": str(index),
            "call_address": address, "slot_offset": "0x80",
            "truncation": "-",
        }
        token = f"def:LOAD@{load + 0x10000:08x}:1:4"
        rows.append(dict(common, row_type="summary",
                         structural_receiver_token=token, recovery_status="canonical_receiver",
                         node_count="5", truncated_by_budget="0",
                         depth_truncation_count="0", cycle_count="0"))
        for n, opcode, parent, input_index in (
            (0, "LOAD", "-1", "-1"), (1, "CAST", "0", "1"),
            (2, "INT_ADD", "1", "0"), (3, "-", "2", "0"), (4, "-", "2", "1"),
        ):
            rows.append(dict(common, row_type="node", node_id=str(n),
                             defining_opcode=opcode, parent_node_id=parent,
                             input_index=input_index,
                             node_token=token if n == 0 else
                             "register:register:00000020:4" if n == 3 else
                             f"node-{index}-{n}",
                             defining_address=f"{load + 0x10000:08x}" if n == 0 else "-",
                             constant_value=f"0x{field:x}" if n == 4 else "-"))
    return inference, rows


def rejected(inference, rows):
    try:
        verify_slices(rows, inference)
    except ValueError:
        return
    raise AssertionError("malformed receiver evidence was accepted")


def main():
    inf, rows = fake_evidence()
    result = verify_slices(rows, inf)
    assert len(result) == 8
    assert {x["receiver_field_offset"] for x in result} == {"0x12c", "0x130", "0x134"}
    assert all(x["receiver_class_proven"] is False and x["affects_routing"] is False for x in result)
    assert all(x["constructor_linked_vtable_id"] == "libcocos2dcpp.so:008f46f8" for x in result)
    assert all(x["constructor_linked_slot_target_id"] == "libcocos2dcpp.so:00525d2e" for x in result)
    assert len(FACTORY_STORES) == 3
    bad = copy.deepcopy(rows)
    bad[5]["constant_value"] = "0x130"
    rejected(inf, bad)
    bad = copy.deepcopy(rows)
    bad[0]["truncated_by_budget"] = "1"
    rejected(inf, bad)
    bad = copy.deepcopy(rows)
    bad[3]["parent_node_id"] = "0"
    rejected(inf, bad)
    bad = copy.deepcopy(rows)
    bad[2]["defining_address"] = "002a0000"
    rejected(inf, bad)
    bad = copy.deepcopy(rows)
    bad[-2]["node_token"] = "register:register:00000030:4"
    rejected(inf, bad)
    bad_inf = copy.deepcopy(inf)
    bad_inf[3]["status"] = "resolved_converged_target"
    rejected(bad_inf, rows)
    bad_inf = copy.deepcopy(inf)
    bad_inf[4]["call_address"] = "002a0000"
    rejected(bad_inf, rows)
    rejected(inf[:7], rows)
    rejected(inf, rows[:6])
    try:
        require(False, "failure")
    except ValueError:
        pass
    else:
        raise AssertionError("require() failed to close")
    print("ControlsButton field receiver witness smoke: ok")


if __name__ == "__main__":
    main()
