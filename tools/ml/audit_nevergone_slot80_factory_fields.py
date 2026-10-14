#!/usr/bin/env python3
"""Exact-APK Thumb factory STORE / Ghidra receiver LOAD candidates, never routing proof."""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
import zipfile
from collections import Counter
from pathlib import Path

SO_NAME = "lib/armeabi-v7a/libcocos2dcpp.so"
SO_SHA256 = "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"
FMT = "NEVERGONE.Slot80FactoryFieldCandidate/1"
SLICE_FMT = "NEVERGONE.GhidraStructuralReceiverSlice/1"
CAT_FMT = "NEVERGONE.StaticVtableSlotCatalog/3"
PLAN_FMT = "NEVERGONE.Slot80ReceiverSlicePlan/1"

# Ghidra source function entry -> (owner, exact unresolved call-index: field).
SOURCES = {
    "002a74c0": ("Controls", {0: 0x12c, 1: 0x130, 2: 0x134, 3: 0x124}),
    "002a7594": ("Controls", {0: 0x12c, 1: 0x130, 2: 0x134, 5: 0x12c, 6: 0x134}),
    "002a7696": ("Controls", {0: 0x124, 1: 0x174}),
    "002a78cc": ("Controls", {0: 0x12c, 1: 0x130, 2: 0x134}),
    "002a87d2": ("ControlsButton", {3: 0x104, 4: 0x10c, 5: 0x108}),
}

# owner, field, created class, factory ELF entry, CALL ELF address, STR.W ELF
# address, SHA-256 of original Thumb bytes [CALL, STR.W + 4).
WITNESSES = [
    ("Controls", 0x124, "CCJoyStick", 0x296670, 0x29805e, 0x298066, "4574916f776aa47665c06e0bd3921c4534a9af148d528b0026a19086ac2a9f94"),
    ("Controls", 0x174, "cocos2d::CCSprite", 0x5396e2, 0x2980da, 0x2980e0, "3fd0f951d9e497cb4c2e8cc32e5a2ca49dddb8c72648739f4a87c80cffbd4924"),
    ("Controls", 0x12c, "ControlsButton", 0x2985fc, 0x298118, 0x298120, "3cb6ec472449e31b12d40939e488f6b1084e6f3e1bc609184c62eaa5be4e39dc"),
    ("Controls", 0x130, "ControlsButton", 0x2985fc, 0x29816a, 0x298172, "55fa160a52a86e3295d4d5932efb837c53b82933f20194c605b2f5a8ac74b300"),
    ("Controls", 0x134, "ControlsButton", 0x2985fc, 0x2981bc, 0x2981c2, "047d2fbe39e83cbb93086062733b56a8584235a5da385045753b75db0cb2b840"),
    ("ControlsButton", 0x104, "cocos2d::CCSprite", 0x539614, 0x2984c6, 0x2984cc, "96505cd61af7685d75f9f05702c0aa90e329a51c99b355925961f22e2160222d"),
    ("ControlsButton", 0x108, "cocos2d::CCSprite", 0x539614, 0x2984e2, 0x2984ea, "1803a846a82ed587583cbb7f681473c9613fdb1c9f4548557496945523806c6d"),
    ("ControlsButton", 0x10c, "cocos2d::CCSprite", 0x539614, 0x298508, 0x298510, "45baa628cf77657c7017628607873f8016100ffa28c4b6f033c5a03fcc55ce30"),
    ("ControlsButton", 0x104, "cocos2d::CCSprite", 0x539614, 0x2985be, 0x2985c4, "6635e448b92b9482f4b42615f6ffe0a24b947684b0c82a78d7b20670650a6943"),
    ("ControlsButton", 0x108, "cocos2d::CCSprite", 0x539614, 0x2985d6, 0x2985de, "c820baafd01f318c3a505883cf6e35652a4af7a7f0668ffd2360d0402fb580fe"),
]
FACTORY_SYMBOLS = {
    0x296670: "_ZN10CCJoyStick6createEiibbbb",
    0x5396e2: "_ZN7cocos2d8CCSprite6createEv",
    0x2985fc: "_ZN14ControlsButton6createEPKcS1_PN7cocos2d8CCObjectEMS3_FvvE",
    0x539614: "_ZN7cocos2d8CCSprite6createEPKc",
}


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


class OriginalELF:
    def __init__(self, data: bytes):
        if hashlib.sha256(data).hexdigest() != SO_SHA256:
            raise ValueError("original ELF SHA-256 mismatch")
        if data[:6] != b"\x7fELF\x01\x01" or len(data) < 52:
            raise ValueError("expected ELF32 little endian")
        self.data = data
        phoff, = struct.unpack_from("<I", data, 28)
        phentsize, phnum = struct.unpack_from("<HH", data, 42)
        self.loads = []
        for i in range(phnum):
            kind, off, va, _, size, _, _, _ = struct.unpack_from("<IIIIIIII", data, phoff + i * phentsize)
            if kind == 1:
                self.loads.append((va, va + size, off))
        shoff, = struct.unpack_from("<I", data, 32)
        shentsize, shnum = struct.unpack_from("<HH", data, 46)
        sections = [struct.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize) for i in range(shnum)]
        self.symbols = {}
        for sec in sections:
            if sec[1] not in (2, 11) or sec[9] != 16:
                continue
            strings = sections[sec[6]]
            strtab = data[strings[4]:strings[4] + strings[5]]
            for at in range(sec[4], sec[4] + sec[5], 16):
                name_off, value, _, info, _, _ = struct.unpack_from("<IIIBBH", data, at)
                if (info & 0xf) != 2 or name_off >= len(strtab):
                    continue
                name = strtab[name_off:].split(b"\0", 1)[0].decode("utf-8", "replace")
                self.symbols[value & ~1] = name

    def at(self, va: int, n: int) -> bytes:
        for start, end, offset in self.loads:
            if start <= va and va + n <= end:
                return self.data[offset + va - start:offset + va - start + n]
        raise ValueError(f"ELF address outside file-backed LOAD: 0x{va:x}")


def decode_bl(elf: OriginalELF, va: int) -> int:
    first, second = struct.unpack("<HH", elf.at(va, 4))
    if first & 0xf800 != 0xf000 or second & 0xd000 != 0xd000:
        raise ValueError(f"not Thumb BL at 0x{va:x}")
    s, j1, j2 = (first >> 10) & 1, (second >> 13) & 1, (second >> 11) & 1
    i1, i2 = 1 ^ j1 ^ s, 1 ^ j2 ^ s
    delta = (s << 24) | (i1 << 23) | (i2 << 22) | ((first & 0x3ff) << 12) | ((second & 0x7ff) << 1)
    if delta & (1 << 24):
        delta -= 1 << 25
    return va + 4 + delta


def decode_strw(elf: OriginalELF, va: int) -> tuple[int, int, int]:
    first, second = struct.unpack("<HH", elf.at(va, 4))
    if first & 0xfff0 != 0xf8c0:
        raise ValueError(f"not Thumb STR.W imm12 at 0x{va:x}")
    return first & 0xf, (second >> 12) & 0xf, second & 0xfff


def verify_witnesses(elf: OriginalELF) -> dict[tuple[str, int], dict]:
    result: dict[tuple[str, int], dict] = {}
    for owner, field, cls, factory, call, store, digest in WITNESSES:
        blob = elf.at(call, store + 4 - call)
        if hashlib.sha256(blob).hexdigest() != digest:
            raise ValueError(f"Thumb instruction window mismatch at 0x{call:x}")
        if decode_bl(elf, call) != factory or decode_strw(elf, store) != (4, 0, field):
            raise ValueError(f"factory return STORE witness invalid: 0x{call:x}/0x{store:x}")
        if elf.symbols.get(factory) != FACTORY_SYMBOLS[factory]:
            raise ValueError(f"factory symbol identity mismatch at 0x{factory:x}")
        init = 0x297e2c if owner == "Controls" else (0x298484 if call < 0x298530 else 0x298588)
        expected_symbol = ("_ZN8Controls4initEv" if owner == "Controls"
                           else "_ZN14ControlsButton19initWithNormalImage")
        if not elf.symbols.get(init, "").startswith(expected_symbol):
            raise ValueError(f"owner initializer identity mismatch at 0x{init:x}")
        key = owner, field
        entry = result.setdefault(key, {"class_name": cls, "witnesses": []})
        if entry["class_name"] != cls:
            raise ValueError(f"divergent factory classes at {key}")
        entry["witnesses"].append({
            "call_elf": f"0x{call:x}", "store_elf": f"0x{store:x}",
            "factory_entry_elf": f"0x{factory:x}", "window_sha256": digest,
        })
    return result


def field_offset(rows: list[dict[str, str]], call: int, address: str, entry: str) -> int:
    matched = [r for r in rows if r.get("call_index") == str(call)]
    summaries = [r for r in matched if r.get("row_type") == "summary"]
    nodes = [r for r in matched if r.get("row_type") == "node"]
    if len(summaries) != 1 or len(nodes) not in (5, 6):
        raise ValueError(f"incomplete slice {entry} call {call}")
    summary = summaries[0]
    if any(summary.get(k) != v for k, v in (
        ("format", SLICE_FMT), ("function_entry", entry), ("call_address", address),
        ("slot_offset", "0x80"), ("recovery_status", "canonical_receiver"),
        ("truncated_by_budget", "0"), ("depth_truncation_count", "0"), ("cycle_count", "0"),
        ("node_count", str(len(nodes))),
    )):
        raise ValueError(f"slice summary mismatch {entry} call {call}")
    by_id = {int(r["node_id"]): r for r in nodes}
    if len(by_id) != len(nodes) or set(by_id) != set(range(len(nodes))):
        raise ValueError(f"invalid slice node IDs {entry} call {call}")
    ops = ["LOAD", "CAST", "INT_ADD" if len(nodes) == 5 else "PTRADD"] + ["-"] * (len(nodes) - 3)
    for i, row in by_id.items():
        if (row.get("format") != SLICE_FMT or row.get("function_entry") != entry
                or row.get("call_address") != address or row.get("slot_offset") != "0x80"):
            raise ValueError(f"slice node identity mismatch {entry} call {call}")
        if row.get("truncation") != "-" or row.get("defining_opcode") != ops[i]:
            raise ValueError(f"slice node opcode/truncation mismatch {entry} call {call}")
        parent = -1 if i == 0 else (0 if i == 1 else (1 if i == 2 else 2))
        if int(row.get("parent_node_id", "-999")) != parent:
            raise ValueError(f"slice edge mismatch {entry} call {call}")
    if (by_id[3]["node_token"] != "register:register:00000020:4"
            or by_id[4]["node_token"] != "const:" + by_id[4]["constant_value"] + ":4"):
        raise ValueError(f"base or offset not canonical {entry} call {call}")
    if len(nodes) == 6 and by_id[5]["constant_value"] != "0x1":
        raise ValueError(f"PTRADD scale not one {entry} call {call}")
    return int(by_id[4]["constant_value"], 0)


def catalog_slot(path: Path, classes: set[str]) -> dict[str, dict]:
    found: dict[str, list[dict]] = {c: [] for c in classes}
    with path.open(encoding="utf-8") as f:
        for raw in f:
            if not raw.strip():
                continue
            r = json.loads(raw)
            if r.get("format") != CAT_FMT:
                raise ValueError("catalog format mismatch")
            if r.get("class_name") in found and r.get("dispatch_slot_offset") == "0x80":
                found[r["class_name"]].append(r)
    result = {}
    for cls, rows in found.items():
        if len(rows) != 1:
            raise ValueError(f"non-unique catalog slot for {cls}: {len(rows)}")
        row = rows[0]
        if (row.get("static_status") != "single_function_memory_pointer"
                or row.get("candidate_target_count") != 1
                or len(row.get("candidate_target_ids") or []) != 1
                or row.get("runtime_validation_status") == "runtime_mismatch"
                or row.get("cross_class_transfer_allowed") is not False):
            raise ValueError(f"unsafe catalog slot for {cls}")
        result[cls] = row
    return result


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("apk", type=Path)
    p.add_argument("plan_tsv", type=Path)
    p.add_argument("slice_dir", type=Path)
    p.add_argument("catalog_jsonl", type=Path)
    p.add_argument("--details-output", type=Path, required=True)
    a = p.parse_args()
    with zipfile.ZipFile(a.apk) as z:
        elf = OriginalELF(z.read(SO_NAME))
    stores = verify_witnesses(elf)
    catalog = catalog_slot(a.catalog_jsonl, {item["class_name"] for item in stores.values()})
    plan = read_tsv(a.plan_tsv)
    if len(plan) != len(SOURCES):
        raise ValueError(f"expected {len(SOURCES)} source functions")
    details = []
    for row in plan:
        source = row.get("source_id")
        entry = row.get("source_entry")
        if source != "libcocos2dcpp.so:" + str(entry) or entry not in SOURCES or row.get("format") != PLAN_FMT:
            raise ValueError("unexpected source in plan")
        owner, expected = SOURCES[entry]
        sym = elf.symbols.get(int(entry, 16) - 0x10000, "")
        if not sym.startswith("_ZN" + str(len(owner)) + owner):
            raise ValueError(f"source owner identity mismatch {entry}: {sym}")
        indices = [int(x) for x in row["call_indices"].split(",")]
        addresses = row["call_addresses"].split(",")
        if (len(indices) != len(addresses) or len(indices) != int(row["expected_calls"])
                or set(indices) != set(expected)):
            raise ValueError(f"call set mismatch {entry}")
        path = a.slice_dir / (source.replace(":", "_").replace("/", "_") + ".tsv")
        slices = read_tsv(path)
        for idx, address in zip(indices, addresses):
            actual = field_offset(slices, idx, address, entry)
            if actual != expected[idx]:
                raise ValueError(f"field offset changed {entry} call {idx}: {actual:#x}")
            factory = stores.get((owner, actual))
            if factory is None:
                raise ValueError(f"no factory STORE for {owner} +0x{actual:x}")
            cls = factory["class_name"]
            target = catalog[cls]
            details.append({
                "format": FMT, "source_id": source, "call_index": idx, "call_address": address,
                "dispatch_slot_offset": "0x80", "owner_class": owner,
                "owner_field_offset": f"0x{actual:x}", "factory_class_candidate": cls,
                "factory_witnesses": factory["witnesses"],
                "candidate_receiver_vtable_id": target["receiver_vtable_id"],
                "candidate_target_id": target["candidate_target_ids"][0],
                "field_writes_exhaustive": False, "receiver_class_proven": False,
                "status": "construction_linked_target_candidate", "affects_routing": False,
            })
    if len(details) != 17 or len({(r["source_id"], r["call_index"]) for r in details}) != 17:
        raise ValueError("expected exactly 17 unique candidate rows")
    a.details_output.parent.mkdir(parents=True, exist_ok=True)
    a.details_output.write_text("".join(json.dumps(r, sort_keys=True) + "\n" for r in details), encoding="utf-8")
    print("format:", FMT)
    print("identity-gated Thumb factory witnesses:", len(WITNESSES))
    print("construction-linked receiver calls:", len(details))
    print("candidate classes:", dict(Counter(r["factory_class_candidate"] for r in details)))
    print("candidate targets:", dict(Counter(r["candidate_target_id"] for r in details)))
    print("field writes exhaustive: no; receiver class proven: no; affects routing: no")
    print("output:", a.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
