#!/usr/bin/env python3
"""Recover the 78 Controls-relative call-argument offsets from exact ARM Thumb.

This is an independent native cross-check for the Ghidra offset probe. It
pins the exact original ELF, all 78 previously triaged call sites and the 43
non-zero CCPoint argument preparations. It does not claim that callees write
through a pointer and never changes production routing.
"""
from __future__ import annotations

import argparse
import collections
import hashlib
import json
import struct
import zipfile
from pathlib import Path

SHA = "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"
SOURCE_FORMAT = "NEVERGONE.ControlsFieldAliasTriage/1"
FORMAT = "NEVERGONE.ControlsThisCallNativeOffsets/1"
SHIFT = 0x10000
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

# Every non-zero argument is a CCPoint subobject. The table maps the CALL site
# in Ghidra address space to the exact ADD.W instruction that creates the
# selected r0/r1 argument from the saved Controls* in r4.
_NONZERO = """
002a69ac 002a697e 0 168
002a69b4 002a69b0 0 190
002a69bc 002a69b8 0 198
002a69c4 002a69c0 0 1a0
002a69cc 002a69c8 0 1a8
002a69d4 002a69d0 0 1b0
002a69dc 002a69d8 0 1b8
002a69e4 002a69e0 0 1c0
002a69ec 002a69e8 0 1c8
002a69f4 002a69f0 0 1d0
002a69fc 002a69f8 0 1d8
002a6a04 002a6a00 0 1e0
002a7414 002a740e 0 168
002a7ab2 002a7aac 1 1b8
002a7ae6 002a7ae0 1 1b0
002a7b1a 002a7b14 1 1c0
002a7b4e 002a7b48 1 1a8
002a7b84 002a7b80 1 190
002a7bac 002a7ba8 1 1a0
002a7bd6 002a7bd2 1 198
002a7c78 002a7c72 1 190
002a7c94 002a7c8e 1 1a0
002a7cb0 002a7caa 1 198
002a7cfe 002a7cf8 1 1b8
002a7d20 002a7d1a 1 1b0
002a7d42 002a7d3c 1 1c0
002a7d64 002a7d5e 1 1a8
002a7d86 002a7d80 1 1d8
002a7da8 002a7da2 1 1c8
002a7dca 002a7dc4 1 1e0
002a7dec 002a7de6 1 1d0
002a7ec6 002a7ec2 0 168
002a82c6 002a82c2 0 1a8
002a82da 002a82d6 0 1b0
002a82ee 002a82ea 0 1b8
002a8302 002a82fe 0 1c0
002a8316 002a8312 0 1c8
002a832a 002a8326 0 1d0
002a833e 002a833a 0 1d8
002a8352 002a834e 0 1e0
002a835c 002a8358 0 190
002a8366 002a8362 0 198
002a8370 002a836a 0 1a0
""".strip()
NONZERO = {
    call: {"prep": prep, "argument_index": arg, "offset": int(offset, 16)}
    for call, prep, arg, offset in (line.split() for line in _NONZERO.splitlines())
}

# Exact little-endian Thumb-2 ADD.W encodings observed in the pinned ELF.
ADD_BYTES = {
    ("0", 0x168): "04f5b470",
    ("0", 0x190): "04f5c870",
    ("0", 0x198): "04f5cc70",
    ("0", 0x1A0): "04f5d070",
    ("0", 0x1A8): "04f5d470",
    ("0", 0x1B0): "04f5d870",
    ("0", 0x1B8): "04f5dc70",
    ("0", 0x1C0): "04f5e070",
    ("0", 0x1C8): "04f5e470",
    ("0", 0x1D0): "04f5e870",
    ("0", 0x1D8): "04f5ec70",
    ("0", 0x1E0): "04f5f070",
    ("1", 0x190): "04f5c871",
    ("1", 0x198): "04f5cc71",
    ("1", 0x1A0): "04f5d071",
    ("1", 0x1A8): "04f5d471",
    ("1", 0x1B0): "04f5d871",
    ("1", 0x1B8): "04f5dc71",
    ("1", 0x1C0): "04f5e071",
    ("1", 0x1C8): "04f5e471",
    ("1", 0x1D0): "04f5e871",
    ("1", 0x1D8): "04f5ec71",
    ("1", 0x1E0): "04f5f071",
}

# The first constructor CCPoint address is prepared well before its BL while
# the class vptrs are installed. Pin that complete native interval so a future
# binary cannot silently redefine r0 between ADD.W and the call.
FIRST_POINT_INTERVAL = (
    0x29697E,
    0x2969B0,
    "b4ac12d58f83a496ee4e4de9af0758ed6cbdaa6beddde36829bda564f1feaa99",
)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def read_jsonl(path: Path) -> list[dict]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def call_instruction_kind(data: bytes, elf_address: int) -> str:
    first = struct.unpack_from("<H", data, elf_address)[0]
    # 16-bit BLX register (three CALLIND sites).
    if first & 0xFF80 == 0x4780:
        return "CALLIND"
    second = struct.unpack_from("<H", data, elf_address + 2)[0]
    # Thumb-2 BL or B.W tail-call forms emitted at Ghidra CALL sites.
    if first & 0xF800 == 0xF000 and (second & 0xD000) in (0xD000, 0x9000):
        return "CALL"
    return "other"


def analyze(rows: list[dict], data: bytes) -> list[dict]:
    require(hashlib.sha256(data).hexdigest() == SHA, "original ELF SHA-256 mismatch")
    require(data[:6] == b"\x7fELF\x01\x01", "expected ELF32 little-endian")
    require(len(NONZERO) == 43, "non-zero native witness table must contain 43 sites")

    require(rows and rows[0].get("format") == SOURCE_FORMAT and
            rows[0].get("record_type") == "summary" and
            rows[0].get("elf_sha256") == SHA and
            rows[0].get("this_bearing_call_count") == 78 and
            rows[0].get("callee_categories") == EXPECTED_CATEGORIES,
            "stale Controls call triage summary")
    calls = [row for row in rows if row.get("record_type") == "this_bearing_call_candidate"]
    require(len(calls) == 78, "expected 78 this-bearing call candidates")
    require(collections.Counter(row.get("callee_category") for row in calls) ==
            collections.Counter(EXPECTED_CATEGORIES), "callee category distribution changed")
    require(collections.Counter(row.get("this_argument_indices") for row in calls) ==
            collections.Counter(EXPECTED_ARGUMENT_INDICES), "argument index distribution changed")
    require(len({row["op_address"] for row in calls}) == 78, "duplicate call address")

    observed_addresses = {row["op_address"] for row in calls}
    require(set(NONZERO) <= observed_addresses, "non-zero witness references unknown call site")

    details = []
    category_offsets: dict[str, collections.Counter[str]] = collections.defaultdict(collections.Counter)
    for row in sorted(calls, key=lambda item: int(item["op_address"], 16)):
        address = row["op_address"]
        elf_address = int(address, 16) - SHIFT
        expected_call_kind = row["call_kind"]
        require(call_instruction_kind(data, elf_address) == expected_call_kind,
                "native call opcode mismatch: " + address)

        witness = NONZERO.get(address)
        if witness is None:
            # Manual Thumb ABI audit: every non-CCPoint call passes the
            # unadjusted Controls pointer. Because the complete ELF is
            # identity-pinned, this table cannot float to a different binary.
            require(row["callee_category"] != "ccpoint_subobject_candidate",
                    "CCPoint call missing non-zero witness: " + address)
            offset = 0
            witness_kind = "exact_sha_pinned_unadjusted_controls_this"
        else:
            require(row["callee_category"] == "ccpoint_subobject_candidate",
                    "non-zero witness belongs to non-CCPoint callee: " + address)
            require(row["this_argument_indices"] == witness["argument_index"],
                    "selected argument index mismatch: " + address)
            offset = witness["offset"]
            prep_elf = int(witness["prep"], 16) - SHIFT
            expected_bytes = ADD_BYTES[(witness["argument_index"], offset)]
            require(data[prep_elf:prep_elf + 4].hex() == expected_bytes,
                    "ADD.W Controls+offset witness mismatch: " + address)
            witness_kind = "exact_thumb_add_from_saved_controls_this"

        category_offsets[row["callee_category"]][hex(offset)] += 1
        details.append({
            "format": FORMAT,
            "record_type": "call_argument",
            "function_entry": row["function_entry"],
            "op_address": address,
            "call_kind": row["call_kind"],
            "callee_category": row["callee_category"],
            "callee_symbol": row.get("callee_symbol"),
            "argument_index": row["this_argument_indices"],
            "controls_relative_offset": hex(offset),
            "native_witness_kind": witness_kind,
            "argument_base_offset_proven": True,
            "callee_write_through_proven": False,
            "affects_routing": False,
        })

    start, end, digest = FIRST_POINT_INTERVAL
    require(hashlib.sha256(data[start:end]).hexdigest() == digest,
            "first constructor CCPoint argument interval changed")

    offsets = collections.Counter(item["controls_relative_offset"] for item in details)
    require(offsets["0x0"] == 35 and sum(v for k, v in offsets.items() if k != "0x0") == 43,
            "35/43 native offset partition changed")
    require(set(category_offsets) == set(EXPECTED_CATEGORIES), "missing category offset result")
    require(category_offsets["ccpoint_subobject_candidate"].get("0x0", 0) == 0,
            "CCPoint candidate unexpectedly receives unadjusted Controls this")
    for category in set(EXPECTED_CATEGORIES) - {"ccpoint_subobject_candidate"}:
        require(category_offsets[category] == {"0x0": EXPECTED_CATEGORIES[category]},
                "non-CCPoint callee receives adjusted Controls pointer: " + category)

    summary = {
        "format": FORMAT,
        "record_type": "summary",
        "elf_sha256": SHA,
        "call_count": len(details),
        "unadjusted_controls_this_calls": offsets["0x0"],
        "nonzero_controls_relative_calls": 43,
        "unresolved_offsets": 0,
        "category_offsets": {
            category: dict(sorted(counter.items()))
            for category, counter in sorted(category_offsets.items())
        },
        "offset_counts": dict(sorted(offsets.items())),
        "all_ccpoint_calls_are_nonzero_subobjects": True,
        "all_non_ccpoint_calls_are_unadjusted_controls_this": True,
        "callee_write_through_proven": False,
        "whole_program_alias_writes_excluded": False,
        "receiver_class_proven": False,
        "affects_routing": False,
    }
    return [summary, *details]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("triage_jsonl", type=Path)
    parser.add_argument("original_apk_or_elf", type=Path)
    parser.add_argument("--details-output", required=True, type=Path)
    args = parser.parse_args()

    if zipfile.is_zipfile(args.original_apk_or_elf):
        with zipfile.ZipFile(args.original_apk_or_elf) as archive:
            data = archive.read("lib/armeabi-v7a/libcocos2dcpp.so")
    else:
        data = args.original_apk_or_elf.read_bytes()

    result = analyze(read_jsonl(args.triage_jsonl), data)
    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    args.details_output.write_text(
        "".join(json.dumps(row, sort_keys=True) + "\n" for row in result),
        encoding="utf-8",
    )
    summary = result[0]
    print("format:", FORMAT)
    print("calls:", summary["call_count"])
    print("unadjusted Controls this:", summary["unadjusted_controls_this_calls"])
    print("non-zero CCPoint subobjects:", summary["nonzero_controls_relative_calls"])
    print("unresolved offsets:", summary["unresolved_offsets"])
    print("category offsets:", summary["category_offsets"])
    print("whole-program alias exclusion: no; affects routing: no")
    print("output:", args.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
