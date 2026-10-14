#!/usr/bin/env python3
"""Synthetic checks: Thumb instruction decoding and fail-closed Ghidra field shapes."""
from __future__ import annotations

import copy
from audit_nevergone_slot80_factory_fields import (
    SLICE_FMT, decode_bl, decode_strw, field_offset,
)


class FakeELF:
    def __init__(self, blob: bytes):
        self.blob = blob

    def at(self, va: int, size: int) -> bytes:
        return self.blob[va:va + size]


def main() -> int:
    assert decode_bl(FakeELF(bytes.fromhex("fef707fb")), 0) == -0x19f2 + 4
    assert decode_strw(FakeELF(bytes.fromhex("c4f82401")), 0) == (4, 0, 0x124)

    base = {
        "format": SLICE_FMT, "function_entry": "002a74c0", "call_address": "002a74fc",
        "slot_offset": "0x80", "truncation": "-", "defining_opcode": "-",
        "constant_value": "-", "node_token": "-", "node_id": "-",
        "parent_node_id": "-", "row_type": "node", "call_index": "0",
    }
    rows = []
    for i, (parent, op) in enumerate([
        (-1, "LOAD"), (0, "CAST"), (1, "INT_ADD"), (2, "-"), (2, "-"),
    ]):
        item = dict(base, node_id=str(i), parent_node_id=str(parent), defining_opcode=op)
        if i == 3:
            item["node_token"] = "register:register:00000020:4"
        if i == 4:
            item.update(node_token="const:0x12c:4", constant_value="0x12c")
        rows.append(item)
    rows.append(dict(
        base, row_type="summary", node_count="5", recovery_status="canonical_receiver",
        truncated_by_budget="0", depth_truncation_count="0", cycle_count="0",
    ))
    assert field_offset(rows, 0, "002a74fc", "002a74c0") == 0x12c

    for mutation in (
        lambda x: x[4].update(constant_value="0x134"),
        lambda x: x[0].update(call_address="002a74fe"),
        lambda x: x[2].update(defining_opcode="INT_SUB"),
        lambda x: x[-1].update(truncated_by_budget="1"),
    ):
        invalid = copy.deepcopy(rows)
        mutation(invalid)
        try:
            field_offset(invalid, 0, "002a74fc", "002a74c0")
        except ValueError:
            pass
        else:
            raise AssertionError("tampered Ghidra receiver field evidence accepted")
    print("slot80 factory field candidate smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
