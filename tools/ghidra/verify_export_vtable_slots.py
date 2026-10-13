#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    source = (root / "tools/ghidra/ExportVtableSlots.java").read_text(encoding="utf-8")
    operands = (root / "tools/ghidra/ExportFunctionOperands.java").read_text(encoding="utf-8")

    required = [
        "ADDRESS_POINT_DELTA = 8",
        "POINTER_SIZE = 4",
        "UINT32_MASK = 0xffffffffL",
        "memory.getInt(slotAddress)",
        "(rawValue & UINT32_MASK) & ~1L",
        "currentProgram.getImageBase().getOffset()",
        "checkedAddress(canonical)",
        "checkedBiasedOffset(canonical, imageBase)",
        "space.getMaxAddress().getOffset()",
        "Long.compareUnsigned(canonical, remaining) > 0",
        "Long.compareUnsigned(slotLong, nextStart) >= 0",
        "VTABLE_SLOT_FORMAT=NEVERGONE.GhidraVtableSlots/1",
        "vtable_symbol_start\\tvtable_symbol_name\\taddress_point\\tslot_offset",
        "resolution_mode",
        "next_vtable_symbol_start",
    ]
    missing = [needle for needle in required if needle not in source]
    if missing:
        raise AssertionError(f"ExportVtableSlots.java missing contract fragments: {missing}")

    forbidden = [
        "functions.getFunctionAt(toAddr(canonical))",
        "functions.getFunctionAt(toAddr(biased))",
        "setBytes(",
        "setInt(",
        "createFunction(",
        "removeFunction(",
        "clearListing(",
    ]
    present = [needle for needle in forbidden if needle in source]
    if present:
        raise AssertionError(f"ExportVtableSlots.java contains forbidden fragments: {present}")

    if "scalar.bitLength()" not in operands:
        raise AssertionError("ExportFunctionOperands.java must use Scalar.bitLength()")
    if "scalar.getBitLength()" in operands:
        raise AssertionError("ExportFunctionOperands.java still uses removed Scalar.getBitLength()")

    print("vtable slot exporter contract: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
