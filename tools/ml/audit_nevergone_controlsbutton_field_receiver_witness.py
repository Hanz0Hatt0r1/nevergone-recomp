#!/usr/bin/env python3
"""Join exact ARMv7 ControlsButton factory/store witnesses to actual CALLIND receivers.

Evidence-only: constructor-linked fields are candidate receiver classes, not
exclusive write-provenance or permission to change production routing.
The original APK/.so is an input and is never copied to repository outputs.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
import zipfile
from pathlib import Path
from typing import Any

ELF_SHA256 = "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"
FORMAT = "NEVERGONE.ControlsButtonFieldReceiverWitness/1"
SOURCE = "libcocos2dcpp.so:002a7594"
GHIDRA_SHIFT = 0x10000
VTABLE_ID = "libcocos2dcpp.so:008f46f8"
TARGET_ID = "libcocos2dcpp.so:00525d2e"
SLOT = 0x80

# call index -> (ELF CALLIND address, ELF receiver LOAD address, field offset)
CALLS = {
    0: (0x2975b4, 0x2975aa, 0x12c),
    1: (0x2975c2, 0x2975b6, 0x130),
    2: (0x2975d0, 0x2975c4, 0x134),
    3: (0x297612, 0x297608, 0x134),
    4: (0x297634, 0x29762a, 0x130),
    5: (0x29764a, 0x29763e, 0x12c),
    6: (0x297658, 0x29764c, 0x134),
    7: (0x297686, 0x29767c, 0x12c),
}

# Controls::init: BL ControlsButton::create; non-clobbering instructions;
# STR.W r0, [r4, #field]. Thumb bytes from the pinned original ELF.
FACTORY_STORES = {
    0x12c: (0x298118, bytes.fromhex("00f070fa41468246c4f82c01"), 0x298120),
    0x130: (0x29816a, bytes.fromhex("00f047fa31468246c4f83001"), 0x298172),
    0x134: (0x2981bc, bytes.fromhex("00f01efa0321c4f83401"), 0x2981c2),
}
SYMBOLS = {
    "_ZN8Controls21showControlsAttackKeyEi": 0x297594,
    "_ZN8Controls4initEv": 0x297e2c,
    "_ZN14ControlsButton6createEPKcS1_PN7cocos2d8CCObjectEMS3_FvvE": 0x2985fc,
    "_ZN14ControlsButtonC2Ev": 0x298448,
    "_ZTV14ControlsButton": 0x8e46f0,
    "_ZN7cocos2d6CCNode10setVisibleEb": 0x515d2e,
}


class OriginalElf:
    def __init__(self, data: bytes):
        if (len(data) < 52 or data[:16] != b"\x7fELF\x01\x01\x01\x00" + b"\x00" * 8
                or struct.unpack_from("<H", data, 18)[0] != 40):
            raise ValueError("not ELF32 little-endian ARM")
        self.data = data
        shoff = struct.unpack_from("<I", data, 32)[0]
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 46)
        if shentsize != 40 or not 0 < shnum < 256 or shstrndx >= shnum:
            raise ValueError("invalid ELF section headers")
        entries = [struct.unpack_from("<IIIIIIIIII", data, shoff + 40 * i)
                   for i in range(shnum)]
        names = entries[shstrndx]
        strtab = data[names[4]:names[4] + names[5]]
        self.sections: dict[str, tuple[int, ...]] = {}
        for entry in entries:
            off = entry[0]
            if off >= len(strtab):
                raise ValueError("invalid section name offset")
            name = strtab[off:strtab.find(b"\x00", off)].decode("ascii")
            if name in self.sections:
                raise ValueError("duplicate ELF section name")
            self.sections[name] = entry

    def section_bytes(self, name: str) -> bytes:
        e = self.sections[name]
        return self.data[e[4]:e[4] + e[5]]

    def at(self, addr: int, length: int) -> bytes:
        for e in self.sections.values():
            # ELF32 sh_type=1 is PROGBITS. sh_addr=entry[3],
            # sh_offset=entry[4], sh_size=entry[5].
            if e[1] != 1:
                continue
            start, file_offset, size = e[3], e[4], e[5]
            if start <= addr and addr + length <= start + size:
                offset = file_offset + addr - start
                return self.data[offset:offset + length]
        raise ValueError(f"unmapped ELF virtual address: {addr:08x}")

    def u32(self, addr: int) -> int:
        return struct.unpack("<I", self.at(addr, 4))[0]

    def symbols(self) -> dict[str, int]:
        dynsym = self.section_bytes(".dynsym")
        dynstr = self.section_bytes(".dynstr")
        result: dict[str, int] = {}
        for pos in range(0, len(dynsym), 16):
            name_off, value, _size, _info, _other, _shndx = struct.unpack_from("<IIIBBH", dynsym, pos)
            if name_off >= len(dynstr):
                raise ValueError("invalid dynamic symbol string offset")
            name = dynstr[name_off:dynstr.find(b"\x00", name_off)].decode("utf-8")
            if name in SYMBOLS:
                result[name] = value
        return result

    def relative_relocation(self, addr: int) -> bool:
        rel = self.section_bytes(".rel.dyn")
        return sum(1 for pos in range(0, len(rel), 8)
                   if struct.unpack_from("<II", rel, pos) == (addr, 23)) == 1


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def verify_binary(elf: OriginalElf) -> None:
    require(hashlib.sha256(elf.data).hexdigest() == ELF_SHA256, "original ELF SHA-256 mismatch")
    symbols = elf.symbols()
    for name, expected in SYMBOLS.items():
        require((symbols.get(name, -1) & ~1) == expected, f"symbol mismatch: {name}")
    require(elf.at(0x297596, 2) == bytes.fromhex("0446"), "Controls::showControlsAttackKey this register not preserved")
    for call, (site, load, field) in CALLS.items():
        require(elf.at(load, 4) in (
            bytes.fromhex(f"d0f8{field & 0xff:02x}{field >> 8:02x}"),
            bytes.fromhex(f"d4f8{field & 0xff:02x}{field >> 8:02x}"),
        ), f"call {call}: receiver field load mismatch")
        require(elf.at(site - 4, 4) == bytes.fromhex("d3f88030"), f"call {call}: vtable slot load mismatch")
        require(elf.at(site, 2) == bytes.fromhex("9847"), f"call {call}: not BLX r3")
    for field, (site, signature, store) in FACTORY_STORES.items():
        require(elf.at(site, len(signature)) == signature, f"field 0x{field:x}: factory-to-store witness mismatch")
        require(site < store < site + len(signature), f"field 0x{field:x}: store location mismatch")
    require(elf.at(0x298614, 4) == bytes.fromhex("fff718ff"), "ControlsButton::create constructor call mismatch")
    require(elf.at(0x298450, 16) == bytes.fromhex("0b4b04f590707b441b6803f108022260"),
            "ControlsButton constructor vptr assignment mismatch")
    # Thumb PC-relative literal at 0x298480 + ADD r3,pc at 0x298456.
    got = elf.u32(0x298480) + 0x29845a
    require(got == 0x92ee20 and elf.relative_relocation(got), "constructor vtable GOT provenance mismatch")
    require(elf.u32(got) == 0x8e46f0, "constructor GOT does not point to ControlsButton vtable")
    vtable_slot = 0x8e46f0 + 8 + SLOT
    require(vtable_slot == 0x8e4778 and elf.relative_relocation(vtable_slot),
            "ControlsButton vtable slot relocation mismatch")
    require(elf.u32(vtable_slot) == 0x515d2f, "ControlsButton slot 0x80 target mismatch")


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    return [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines() if line.strip()]


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def verify_slices(rows: list[dict[str, str]], inference: list[dict[str, Any]]) -> list[dict[str, Any]]:
    selected: dict[int, dict[str, Any]] = {}
    for row in inference:
        if row.get("source_id") != SOURCE or row.get("call_index") not in CALLS:
            continue
        call = row["call_index"]
        require(call not in selected, f"duplicate inference call {call}")
        selected[call] = row
    require(set(selected) == set(CALLS), "inference missing Controls::showControlsAttackKey calls")
    bases: set[str] = set()
    details: list[dict[str, Any]] = []
    for call, (site, load, field) in CALLS.items():
        inf = selected[call]
        address = f"{site + GHIDRA_SHIFT:08x}"
        require(inf.get("call_address") == address
                and inf.get("dispatch_slot_offset") == "0x80"
                and inf.get("status") in {"unresolved_receiver_class", "receiver_not_single_pointer"},
                f"call {call}: stale or mismatched v11 inference")
        group = [r for r in rows if r.get("call_index") == str(call)]
        require(len(group) == 6, f"call {call}: expected one summary and five nodes")
        require(all(r.get("format") == "NEVERGONE.GhidraStructuralReceiverSlice/1"
                    and r.get("function_entry") == "002a7594"
                    and r.get("call_address") == address
                    and r.get("slot_offset") == "0x80" for r in group),
                f"call {call}: Ghidra identity mismatch")
        summary = [r for r in group if r.get("row_type") == "summary"]
        nodes = {r.get("node_id"): r for r in group if r.get("row_type") == "node"}
        require(len(summary) == 1 and set(nodes) == {"0", "1", "2", "3", "4"},
                f"call {call}: malformed receiver tree")
        s = summary[0]
        require(s.get("recovery_status") == "canonical_receiver"
                and s.get("node_count") == "5"
                and all(s.get(key) == "0" for key in (
                    "truncated_by_budget", "depth_truncation_count", "cycle_count")),
                f"call {call}: incomplete receiver tree")
        expected_nodes = {
            "0": ("LOAD", "-1", "-1"),
            "1": ("CAST", "0", "1"),
            "2": ("INT_ADD", "1", "0"),
            "3": ("-", "2", "0"),
            "4": ("-", "2", "1"),
        }
        for key, (opcode, parent, input_index) in expected_nodes.items():
            n = nodes[key]
            require(n.get("defining_opcode") == opcode
                    and n.get("parent_node_id") == parent
                    and n.get("input_index") == input_index
                    and n.get("truncation") == "-",
                    f"call {call}: unexpected node {key}")
        require(nodes["0"].get("node_token") == s.get("structural_receiver_token")
                and nodes["0"].get("defining_address") == f"{load + GHIDRA_SHIFT:08x}",
                f"call {call}: receiver root mismatch")
        require(nodes["4"].get("constant_value") == f"0x{field:x}",
                f"call {call}: receiver field mismatch")
        base = nodes["3"].get("node_token")
        require(isinstance(base, str) and base.startswith("register:"), f"call {call}: not register-rooted")
        bases.add(base)
        details.append({
            "format": FORMAT, "source_id": SOURCE, "call_index": call,
            "call_address": address, "dispatch_slot_offset": "0x80",
            "receiver_field_offset": f"0x{field:x}",
            "field_store_address": f"{FACTORY_STORES[field][2] + GHIDRA_SHIFT:08x}",
            "factory_function": "ControlsButton::create",
            "factory_constructor": "ControlsButton::ControlsButton",
            "constructor_linked_class_candidate": "ControlsButton",
            "constructor_linked_vtable_id": VTABLE_ID,
            "constructor_linked_slot_target_id": TARGET_ID,
            "receiver_class_proven": False, "all_field_writers_excluded": False,
            "status": "constructor_linked_field_receiver_candidate",
            "affects_routing": False,
        })
    require(len(bases) == 1, "receiver fields do not share a common base register")
    return details


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("original_apk_or_elf", type=Path)
    p.add_argument("v11_inference_jsonl", type=Path)
    p.add_argument("ghidra_receiver_slices_tsv", type=Path)
    p.add_argument("--details-output", required=True, type=Path)
    a = p.parse_args()
    if zipfile.is_zipfile(a.original_apk_or_elf):
        with zipfile.ZipFile(a.original_apk_or_elf) as archive:
            data = archive.read("lib/armeabi-v7a/libcocos2dcpp.so")
    else:
        data = a.original_apk_or_elf.read_bytes()
    verify_binary(OriginalElf(data))
    details = verify_slices(read_tsv(a.ghidra_receiver_slices_tsv),
                            read_jsonl(a.v11_inference_jsonl))
    a.details_output.parent.mkdir(parents=True, exist_ok=True)
    a.details_output.write_text("".join(json.dumps(row, sort_keys=True) + "\n" for row in details),
                                encoding="utf-8")
    print("format:", FORMAT)
    print("exact original ELF SHA-256: matched")
    print("constructor -> ControlsButton vtable -> slot 0x80: matched")
    print("factory -> Controls fields 0x12c, 0x130, 0x134: matched")
    print("actual CALLIND receiver field reads: 8/8 matched")
    print("all writers excluded: no; affects routing: no")
    print("output:", a.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
