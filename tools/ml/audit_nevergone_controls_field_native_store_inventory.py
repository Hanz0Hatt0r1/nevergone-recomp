#!/usr/bin/env python3
"""Inventory ARM Thumb STR.W immediate-offset candidates in the original Nevergone ELF.

A matching offset in another class is NOT a write to Controls. This is a
coarse whole-ELF screen, not alias completeness or receiver-class proof.
"""
from __future__ import annotations

import argparse
import csv
import hashlib
import json
import struct
import zipfile
from pathlib import Path

from audit_nevergone_controlsbutton_field_receiver_witness import (
    ELF_SHA256, FACTORY_STORES, GHIDRA_SHIFT, OriginalElf, require, verify_binary,
)

FORMAT = "NEVERGONE.ControlsFieldNativeStoreInventory/1"
FIELDS = frozenset(FACTORY_STORES)
EXPECTED_CONTROL_SYMBOLS = 40
EXPECTED_CONTROL_ENTRIES = 38
EXPECTED_CONTROL_WRITES = frozenset(store for _, _, store in FACTORY_STORES.values())


def function_symbols(elf: OriginalElf) -> list[tuple[int, int, str]]:
    data = elf.section_bytes(".dynsym")
    names = elf.section_bytes(".dynstr")
    found: list[tuple[int, int, str]] = []
    require(len(data) % 16 == 0, "ELF dynsym is not a multiple of 16 bytes")
    for pos in range(0, len(data), 16):
        name_off, value, size, info, _other, _shndx = struct.unpack_from(
            "<IIIBBH", data, pos
        )
        if (info & 0x0F) != 2 or not size or not value:
            continue
        require(name_off < len(names), "invalid dynsym name offset")
        end = names.find(b"\x00", name_off)
        require(end >= 0, "unterminated dynsym name")
        name = names[name_off:end].decode("utf-8", "replace")
        found.append((value & ~1, size, name))
    return found


def is_controls(name: str) -> bool:
    return name.startswith("_ZN8Controls") or name.startswith("_ZNK8Controls")


def collect(elf: OriginalElf) -> tuple[list[dict], list[dict]]:
    verify_binary(elf)
    symbols = function_symbols(elf)
    controls = [(addr, size, name) for addr, size, name in symbols if is_controls(name)]
    entries = sorted({addr for addr, _size, _name in controls})
    require(len(controls) == EXPECTED_CONTROL_SYMBOLS, "Controls symbol count mismatch")
    require(len(entries) == EXPECTED_CONTROL_ENTRIES, "Controls method count mismatch")

    section = elf.sections[".text"]
    base = section[3]
    code = elf.section_bytes(".text")
    stores: list[dict] = []
    # A Thumb-2 STR.W (immediate unsigned) has first halfword 0xF8C0..0xF8CF,
    # second halfword Rt:4 + imm12. Scanning halfword-aligned bytes can include
    # false positives in literal pools or as a suffix of another instruction.
    for pos in range(0, len(code) - 3, 2):
        high, low = struct.unpack_from("<HH", code, pos)
        offset = low & 0x0FFF
        if (high & 0xFFF0) != 0xF8C0 or offset not in FIELDS:
            continue
        address = base + pos
        owners = sorted({
            name for start, size, name in symbols
            if start <= address < start + size
        })
        control_owners = [name for name in owners if is_controls(name)]
        stores.append({
            "format": FORMAT, "record_type": "store_encoding_candidate",
            "elf_address": f"{address:08x}",
            "ghidra_address": f"{address + GHIDRA_SHIFT:08x}",
            "offset": f"0x{offset:x}",
            "base_register": f"r{high & 0xf}",
            "value_register": f"r{low >> 12}",
            "function_symbols": owners,
            "controls_method": bool(control_owners),
            "known_factory_store": address in EXPECTED_CONTROL_WRITES,
            "instruction_boundary_verified": False,
            "receiver_class_proven": False, "affects_routing": False,
        })

    controls_stores = [s for s in stores if s["controls_method"]]
    require({int(s["elf_address"], 16) for s in controls_stores} == EXPECTED_CONTROL_WRITES,
            "Controls has additional or missing STR.W immediate-offset candidates")
    require(all(s["base_register"] == "r4" and s["value_register"] == "r0"
                for s in controls_stores), "Controls store register mismatch")
    manifest = []
    for addr in entries:
        names = sorted({name for start, _size, name in controls if start == addr})
        manifest.append({
            "function_entry": f"{addr + GHIDRA_SHIFT:08x}",
            "elf_entry": f"{addr:08x}",
            "symbol_names": ";".join(names),
        })
    summary = {
        "format": FORMAT, "record_type": "summary",
        "elf_sha256": hashlib.sha256(elf.data).hexdigest(),
        "controls_symbol_count": len(controls),
        "controls_unique_method_count": len(entries),
        "whole_elf_store_encoding_candidates": len(stores),
        "unowned_encoding_candidates": sum(not s["function_symbols"] for s in stores),
        "controls_store_encoding_candidates": len(controls_stores),
        "controls_store_addresses": sorted(s["ghidra_address"] for s in controls_stores),
        "instruction_boundaries_proven_globally": False,
        "all_alias_writers_excluded": False,
        "receiver_class_proven": False, "affects_routing": False,
    }
    return [summary, *stores], manifest


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("original_apk_or_elf", type=Path)
    p.add_argument("--details-output", type=Path, required=True)
    p.add_argument("--methods-output", type=Path, required=True)
    a = p.parse_args()
    if zipfile.is_zipfile(a.original_apk_or_elf):
        with zipfile.ZipFile(a.original_apk_or_elf) as archive:
            data = archive.read("lib/armeabi-v7a/libcocos2dcpp.so")
    else:
        data = a.original_apk_or_elf.read_bytes()
    require(hashlib.sha256(data).hexdigest() == ELF_SHA256, "original ELF SHA mismatch")
    rows, methods = collect(OriginalElf(data))
    a.details_output.parent.mkdir(parents=True, exist_ok=True)
    a.methods_output.parent.mkdir(parents=True, exist_ok=True)
    a.details_output.write_text(
        "".join(json.dumps(row, sort_keys=True) + "\n" for row in rows), encoding="utf-8"
    )
    with a.methods_output.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle, fieldnames=["function_entry", "elf_entry", "symbol_names"], delimiter="\t"
        )
        writer.writeheader()
        writer.writerows(methods)
    s = rows[0]
    print("format:", FORMAT)
    print("original ELF SHA256:", s["elf_sha256"])
    print("Controls symbol entries / unique method addresses:",
          s["controls_symbol_count"], "/", s["controls_unique_method_count"])
    print("whole ELF STR.W offset encoding candidates:",
          s["whole_elf_store_encoding_candidates"])
    print("without STT_FUNC owner:", s["unowned_encoding_candidates"])
    print("Controls-method direct candidates:", s["controls_store_addresses"])
    print("alias writes excluded: no; affects routing: no")
    print("inventory:", a.details_output)
    print("method manifest:", a.methods_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
