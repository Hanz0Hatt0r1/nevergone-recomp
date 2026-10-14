#!/usr/bin/env python3
"""Synthetic checks for exact native Controls call-argument offsets."""
from collections import Counter
import struct

from audit_nevergone_controls_this_call_native_offsets import (
    ADD_BYTES,
    EXPECTED_ARGUMENT_INDICES,
    EXPECTED_CATEGORIES,
    NONZERO,
    call_instruction_kind,
)


def main() -> int:
    assert sum(EXPECTED_CATEGORIES.values()) == 78
    assert sum(EXPECTED_ARGUMENT_INDICES.values()) == 78
    assert len(NONZERO) == 43
    assert Counter(item["argument_index"] for item in NONZERO.values()) == {"0": 25, "1": 18}
    assert all((item["argument_index"], item["offset"]) in ADD_BYTES for item in NONZERO.values())
    assert all(0x168 <= item["offset"] <= 0x1E0 for item in NONZERO.values())

    # BLX r3, BL and B.W tail-call examples.
    data = bytearray(16)
    struct.pack_into("<H", data, 0, 0x4798)
    struct.pack_into("<HH", data, 4, 0xF7FF, 0xFD87)
    struct.pack_into("<HH", data, 8, 0xF7FF, 0xBD7D)
    struct.pack_into("<HH", data, 12, 0xBF00, 0xBF00)
    assert call_instruction_kind(data, 0) == "CALLIND"
    assert call_instruction_kind(data, 4) == "CALL"
    assert call_instruction_kind(data, 8) == "CALL"
    assert call_instruction_kind(data, 12) == "other"

    print("Controls this-call native offset smoke: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
