#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path


def main() -> int:
    root = Path(__file__).resolve().parents[2]
    source = (root / "tools/ghidra/ExportVtableSlots.java").read_text(encoding="utf-8")

    required = [
        "ADDRESS_POINT_DELTA = 8",
        "POINTER_SIZE = 4",
        "memory.getInt(slotAddress)",
        "rawValue & ~1L",
        "currentProgram.getImageBase().getOffset()",
        "functions.getFunctionAt(toAddr(canonical))",
        "functions.getFunctionAt(toAddr(biased))",
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
        "setBytes(",
        "setInt(",
        "createFunction(",
        "removeFunction(",
        "clearListing(",
    ]
    present = [needle for needle in forbidden if needle in source]
    if present:
        raise AssertionError(f"ExportVtableSlots.java contains mutating API fragments: {present}")

    print("vtable slot exporter contract: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
