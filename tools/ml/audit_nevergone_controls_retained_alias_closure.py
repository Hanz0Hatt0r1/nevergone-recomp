#!/usr/bin/env python3
"""Verify the two retained Controls* callback/alias paths in the original ARM ELF.

Evidence-only. This proves the exact local callback targets reached through
ControlsButton+0x110 and CCJoyStick+0x14c and rejects direct writes from those
callbacks to the retained joystick/action-button pointer fields. It is not a
whole-program alias/write-exclusivity proof and never changes routing.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zipfile
from pathlib import Path

SHA = "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"
FORMAT = "NEVERGONE.ControlsRetainedAliasClosure/1"
DANGEROUS = {0x124, 0x12C, 0x130, 0x134, 0x174}
BUTTON_CALLBACKS = {
    "_ZN8Controls14menuABCallbackEv": (0x2966E0, 8),
    "_ZN8Controls14menuDBCallbackEv": (0x2966E8, 8),
    "_ZN8Controls13menuJCallbackEv": (0x2966F0, 8),
}
DELEGATE_TARGETS = {
    "_ZN8Controls18onCCJoyStickUpdateEPN7cocos2d6CCNodeEfNS0_7CCPointEf": (
        0x2973DC,
        220,
        "07981963c3b0847f1378bda34330b3dd643f1743ae6f8a3dbdfa9f545fe988cf",
    ),
    "_ZN8Controls21onCCJoyStickActivatedEPN7cocos2d6CCNodeE": (
        0x296760,
        40,
        "7405ebb37aa53a231268116a962c6bba30f8b08fd582369b04094560df3e4f9d",
    ),
    "_ZN8Controls23onCCJoyStickDeactivatedEPN7cocos2d6CCNodeE": (
        0x2970D4,
        96,
        "6b7119d914eb7b80c1a915c7b160ef8121aaee5fc0827f6ce9925a32d492f600",
    ),
}
THUNKS = {
    0x0: (
        "_ZThn260_N8Controls18onCCJoyStickUpdateEPN7cocos2d6CCNodeEfNS0_7CCPointEf",
        0x2974B8,
    ),
    0x4: (
        "_ZThn260_N8Controls21onCCJoyStickActivatedEPN7cocos2d6CCNodeE",
        0x296788,
    ),
    0x8: (
        "_ZThn260_N8Controls23onCCJoyStickDeactivatedEPN7cocos2d6CCNodeE",
        0x297134,
    ),
}
CCJOY_14C = {
    0x295DD6: "load",
    0x295EF2: "store",
    0x296254: "load",
    0x296378: "load",
}


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


class OriginalElf:
    def __init__(self, data: bytes):
        require(hashlib.sha256(data).hexdigest() == SHA, "ELF SHA mismatch")
        require(data[:6] == b"\x7fELF\x01\x01", "not ELF32 little-endian")
        self.data = data

        shoff = struct.unpack_from("<I", data, 32)[0]
        shentsize, shnum, shstrndx = struct.unpack_from("<HHH", data, 46)
        require(shentsize == 40 and 0 < shnum < 256, "invalid section table")
        self.sections = [
            struct.unpack_from("<IIIIIIIIII", data, shoff + i * shentsize)
            for i in range(shnum)
        ]
        shstr = self.sections[shstrndx]
        section_names = data[shstr[4] : shstr[4] + shstr[5]]
        self.section_by_name = {}
        for section in self.sections:
            start = section[0]
            end = section_names.find(b"\x00", start)
            require(end >= 0, "unterminated section name")
            self.section_by_name[section_names[start:end].decode()] = section

        dynsym = self.section_by_name[".dynsym"]
        dynstr = self.section_by_name[".dynstr"]
        strings = data[dynstr[4] : dynstr[4] + dynstr[5]]
        self.symbols: dict[str, tuple[int, int, int, int]] = {}
        self.functions: list[tuple[int, int, str]] = []
        for pos in range(dynsym[4], dynsym[4] + dynsym[5], 16):
            name_offset, value, size, info, _other, shndx = struct.unpack_from(
                "<IIIBBH", data, pos
            )
            end = strings.find(b"\x00", name_offset)
            require(end >= 0, "unterminated symbol name")
            name = strings[name_offset:end].decode("utf-8", "replace")
            symbol_type = info & 15
            if name:
                self.symbols[name] = (value, size, symbol_type, shndx)
            if symbol_type == 2 and value and size:
                self.functions.append((value & ~1, size, name))

        self.relative_relocations: dict[int, int] = {}
        for section in self.sections:
            if section[1] != 9:
                continue
            require(section[9] == 8 and section[5] % 8 == 0, "unexpected REL layout")
            for pos in range(section[4], section[4] + section[5], 8):
                address, info = struct.unpack_from("<II", data, pos)
                self.relative_relocations[address] = info & 255

    def file_offset(self, address: int, size: int = 1) -> int:
        matches = [
            section
            for section in self.sections
            if section[1] == 1
            and section[3] <= address
            and address + size <= section[3] + section[5]
        ]
        require(len(matches) == 1, f"unmapped ELF VA 0x{address:x}")
        section = matches[0]
        return section[4] + address - section[3]

    def at(self, address: int, size: int) -> bytes:
        offset = self.file_offset(address, size)
        return self.data[offset : offset + size]

    def u32(self, address: int) -> int:
        return struct.unpack("<I", self.at(address, 4))[0]

    def symbol(self, name: str, address: int | None = None, size: int | None = None):
        require(name in self.symbols, "missing symbol: " + name)
        value, actual_size, symbol_type, _shndx = self.symbols[name]
        require(symbol_type in (1, 2), "unexpected symbol type: " + name)
        if address is not None:
            require((value & ~1) == address, "symbol address mismatch: " + name)
        if size is not None:
            require(actual_size == size, "symbol size mismatch: " + name)
        return value, actual_size


def immediate_stores(elf: OriginalElf, start: int, size: int):
    result = []
    for pc in range(start, start + size - 3, 2):
        first, second = struct.unpack("<HH", elf.at(pc, 4))
        opcode = first & 0xFFF0
        if opcode in (0xF8C0, 0xF880):
            result.append(
                (
                    pc,
                    4 if opcode == 0xF8C0 else 1,
                    first & 15,
                    second >> 12,
                    second & 0xFFF,
                )
            )
    return result


def verify_controlsbutton_callback_path(elf: OriginalElf) -> dict:
    callback_got = [
        (0x92EE10, "_ZN8Controls14menuABCallbackEv"),
        (0x92EE14, "_ZN8Controls14menuDBCallbackEv"),
        (0x92EE18, "_ZN8Controls13menuJCallbackEv"),
    ]
    for got, name in callback_got:
        value, size = elf.symbol(name, *BUTTON_CALLBACKS[name])
        require(elf.relative_relocations.get(got) == 23, "callback GOT relocation mismatch")
        require(elf.u32(got) == value, "callback GOT target mismatch")
        body = elf.at(value & ~1, size)
        require(body.hex() == "0023c0f818317047", "button callback body changed")
        stores = immediate_stores(elf, value & ~1, size)
        require(
            stores == [((value & ~1) + 2, 4, 0, 3, 0x118)],
            "button callback writes an unexpected field",
        )

    # Controls::init keeps the ARM EABI member-function adjustment word at zero.
    require(elf.at(0x297E4A, 4).hex() == "4ff0000b", "Controls::init r11 zero changed")
    preparations = [
        (0x298102, 0x298112, 0x298114, 0x298118),
        (0x298154, 0x298164, 0x298166, 0x29816A),
        (0x2981A6, 0x2981B6, 0x2981B8, 0x2981BC),
    ]
    for object_move, pointer_store, adjust_store, factory_call in preparations:
        require(elf.at(object_move, 2).hex() == "2246", "callback object is not Controls this")
        require(elf.at(pointer_store, 2)[1] & 0xF0 == 0x90, "member pointer not stack-stored")
        require(elf.at(adjust_store, 4)[:2] == bytes.fromhex("cdf8"), "adjuster not stored")
        require(elf.at(factory_call, 2)[1] & 0xF8 == 0xF0, "factory BL encoding changed")

    # create() forwards the callback object; initWithNormalImage() retains the
    # object at +0x110 and the two-word member-function pointer at +0x114/+0x118.
    require(
        elf.at(0x29860A, 2).hex() == "1646" and elf.at(0x29861E, 2).hex() == "3346",
        "ControlsButton callback forwarding changed",
    )
    require(elf.at(0x29858E, 4).hex() == "c0f81031", "callback object +0x110 store changed")
    require(elf.at(0x29859A, 4).hex() == "00f58a73", "member pointer +0x114 destination changed")
    require(elf.at(0x2985A8, 8).hex() == "92e8030083e80300", "member pointer pair copy changed")

    # touchEnd reads the retained pair/object and dispatches it through BLX r2.
    for address, expected in [
        (0x298986, "d4f81421"),
        (0x29898C, "d4f81831"),
        (0x298994, "d4f81031"),
        (0x29899A, "d4f81801"),
        (0x2989B0, "9047"),
    ]:
        require(
            elf.at(address, len(expected) // 2).hex() == expected,
            "touchEnd callback dispatch changed",
        )

    return {
        "callback_object_field": "ControlsButton+0x110",
        "member_pointer_fields": ["ControlsButton+0x114", "ControlsButton+0x118"],
        "targets": list(BUTTON_CALLBACKS),
        "target_only_store": "Controls+0x118",
        "pointer_field_write": False,
    }


def verify_joystick_delegate_path(elf: OriginalElf) -> dict:
    vtable, _size = elf.symbol("_ZTV8Controls", 0x8E4490, 592)
    require(
        elf.relative_relocations.get(0x92EE04) == 23 and elf.u32(0x92EE04) == vtable,
        "Controls vtable GOT mismatch",
    )

    # Controls constructor stores the secondary CCJoyStickDelegate vptr address
    # point (_ZTV8Controls + 0x244) into Controls+0x104.
    require(
        elf.at(0x29699C, 16).hex() == "03f50d7203f51173c4f8e820c4f80431",
        "Controls secondary vptr installation changed",
    )
    address_point = vtable + 0x244
    for slot, (name, target) in THUNKS.items():
        value, _ = elf.symbol(name, target, 8)
        entry = address_point + slot
        require(elf.relative_relocations.get(entry) == 23, "delegate vtable relocation mismatch")
        require(elf.u32(entry) == value, "delegate vtable target mismatch")
        require(elf.at(target, 4).hex() == "a0f58270", "delegate thunk no longer subtracts 0x104")

    # Controls::init stores Controls+0x104 into CCJoyStick+0x14c.
    require(
        elf.at(0x298084, 12).hex() == "d4f8243104f58272c3f84c21",
        "joystick delegate pointer installation changed",
    )

    # Exhaustive immediate word LOAD/STORE inventory for +0x14c across every
    # symbolized CCJoyStick method. The constructor zeroes it; three methods
    # read it and dispatch delegate slots 0/4/8.
    observed = {}
    for start, size, name in elf.functions:
        if not (name.startswith("_ZN10CCJoyStick") or name.startswith("_ZNK10CCJoyStick")):
            continue
        for pc in range(start, start + size - 3, 2):
            first, second = struct.unpack("<HH", elf.at(pc, 4))
            opcode = first & 0xFFF0
            if opcode in (0xF8D0, 0xF8C0) and (second & 0xFFF) == 0x14C:
                observed[pc] = "load" if opcode == 0xF8D0 else "store"
    require(observed == CCJOY_14C, "unexpected CCJoyStick+0x14c access inventory")
    require(elf.at(0x295EEE, 8).hex() == "00232046c4f84c31", "CCJoyStick+0x14c zero init changed")
    require(elf.at(0x295DD6, 4).hex() == "d0f84c51", "timerUpdate delegate load changed")
    require(elf.at(0x295DFC, 4).hex() == "3346b847", "timerUpdate slot-0 dispatch changed")
    require(
        elf.at(0x296254, 14).hex() == "d0f84c0118b1036821465b689847",
        "startTimer slot-4 dispatch changed",
    )
    require(
        elf.at(0x296378, 14).hex() == "d0f84c0118b1036821469b689847",
        "stopTimer slot-8 dispatch changed",
    )

    target_stores = {}
    for name, (address, size, digest) in DELEGATE_TARGETS.items():
        elf.symbol(name, address, size)
        require(hashlib.sha256(elf.at(address, size)).hexdigest() == digest, "delegate body changed: " + name)
        stores = immediate_stores(elf, address, size)
        dangerous = [row for row in stores if row[4] in DANGEROUS]
        require(not dangerous, "delegate target directly writes a retained pointer field: " + name)
        target_stores[name] = [hex(row[4]) for row in stores]

    return {
        "delegate_subobject": "Controls+0x104",
        "joystick_field": "CCJoyStick+0x14c",
        "secondary_vtable_address_point": hex(address_point),
        "slot_targets": {hex(slot): value[0] for slot, value in THUNKS.items()},
        "target_store_offsets": target_stores,
        "pointer_field_write": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original_apk_or_elf", type=Path)
    parser.add_argument("--details-output", required=True, type=Path)
    args = parser.parse_args()

    if zipfile.is_zipfile(args.original_apk_or_elf):
        with zipfile.ZipFile(args.original_apk_or_elf) as archive:
            data = archive.read("lib/armeabi-v7a/libcocos2dcpp.so")
    else:
        data = args.original_apk_or_elf.read_bytes()

    elf = OriginalElf(data)
    button = verify_controlsbutton_callback_path(elf)
    joystick = verify_joystick_delegate_path(elf)
    report = {
        "format": FORMAT,
        "elf_sha256": SHA,
        "controlsbutton_callback_path": button,
        "joystick_delegate_path": joystick,
        "retained_alias_callback_paths_checked": 2,
        "retained_alias_callbacks_write_controls_pointer_fields": False,
        "local_retained_alias_paths_closed": True,
        "whole_program_alias_writes_excluded": False,
        "receiver_class_proven": False,
        "affects_routing": False,
    }
    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    args.details_output.write_text(json.dumps(report, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", FORMAT)
    print("button callbacks:", ", ".join(button["targets"]))
    print("joystick delegate slots:", joystick["slot_targets"])
    print("retained callback paths directly write pointer fields: no")
    print("whole-program alias exclusion: no; affects routing: no")
    print("output:", args.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
