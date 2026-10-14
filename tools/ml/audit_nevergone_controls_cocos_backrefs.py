#!/usr/bin/env python3
"""Verify the remaining known Cocos-retained Controls* back-reference channels.

Evidence-only. This pins the two CCNode parent links created by Controls,
and the inherited CCLayer touch-dispatch registration/retention path. It
classifies the exact retained pointers and inherited native callbacks but does
not claim whole-program alias/write exclusivity or change receiver routing.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
import zipfile
from collections import Counter
from pathlib import Path

from audit_nevergone_controls_retained_alias_closure import OriginalElf, require

FORMAT = "NEVERGONE.ControlsCocosBackrefs/1"
SHA = "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"
DANGEROUS = {0x124, 0x12C, 0x130, 0x134, 0x174}

FUNCTION_HASHES = {
    "_ZN7cocos2d6CCNode8addChildEPS0_": (0x515DBC, 20, "a0eb9ad6ac2dc6a637de8fbf4952c664b260ba389a843325e5c753c09ef48c05"),
    "_ZN7cocos2d6CCNode8addChildEPS0_ii": (0x516C64, 100, "3f521af5ee27d63b7aaa0317f481e39e0dbcd7e77040fdf5cf70b8f4b6f12ee5"),
    "_ZN7cocos2d6CCNode9setParentEPS0_": (0x515D4C, 6, "7d2fab5043c2f13a4694e84cd69bfd258ee035dde971f352183a03dfe2fa559e"),
    "_ZN7cocos2d7CCLayer15setTouchEnabledEb": (0x524DA8, 58, "f956011b464506018389df0ebcc76eaa246d2801f331871853db116e6c58144c"),
    "_ZN7cocos2d7CCLayer27registerWithTouchDispatcherEv": (0x5248F2, 68, "9e54c7f79edf22d7d6756c2e9a9c888802d13523a3aa626b72340ba99af47ec2"),
    "_ZN7cocos2d17CCTouchDispatcher19addStandardDelegateEPNS_15CCTouchDelegateEi": (0x5493F4, 74, "38eb3a1e6a2b11b2bd2a64d34985b396cf62158c8abadb85601665c6a2c1970d"),
    "_ZN7cocos2d22CCStandardTouchHandler19handlerWithDelegateEPNS_15CCTouchDelegateEi": (0x549D68, 76, "14062632e397da576626865d457366bc24296d4fffbac3c232ea504b5347e24c"),
    "_ZN7cocos2d14CCTouchHandler16initWithDelegateEPNS_15CCTouchDelegateEi": (0x549BC4, 56, "2c9d8d8f0b8377b603167b66f45adc68a68c4eb72ab9227ab85630a19da4d989"),
    "_ZN7cocos2d7CCLayer7onEnterEv": (0x525B2A, 82, "82c1979e58f12d8d73dd8f3b4462f54d40e05539a6cb714c8f74f88eb3092a08"),
    "_ZN7cocos2d6CCNodeC2Ev": (0x516970, 248, "c9c4a1128cd8b34cb899e9c6e10743e0eac4002c1a9d17c0e8323c3a79d4cd66"),
    "_ZN7cocos2d8CCSprite6createEPKc": (0x539614, 58, "298db55b0a638fceea6a0204a61f0c53f808fce4142a96d641cb63e4e873c4f2"),
    "_ZN7cocos2d7CCLayer6createEv": (0x525278, 52, "cd972db0ebd410b9f4fd4491cd0a0d3638f0303086b6ec39de4a6d2288f1410f"),
    "_ZN7cocos2d7CCLayer14ccTouchesBeganEPNS_5CCSetEPNS_7CCEventE": (0x525350, 16, "ddee523e11cefb081ba64dd913fdc85f4154d3dfd87de920b8e87d6d5bce935f"),
    "_ZN7cocos2d7CCLayer14ccTouchesMovedEPNS_5CCSetEPNS_7CCEventE": (0x525368, 16, "5060d1104bc183162aa6d10a74d722f09b324ef0198316e75fb2c94528d65a3a"),
    "_ZN7cocos2d7CCLayer14ccTouchesEndedEPNS_5CCSetEPNS_7CCEventE": (0x525380, 16, "e47d1eaa5e65c7ce13303d08e7f522237a921dfa3f4d94f85fbb7ed6f6202694"),
    "_ZN7cocos2d7CCLayer18ccTouchesCancelledEPNS_5CCSetEPNS_7CCEventE": (0x525398, 16, "6d8a7a2883cc201e015a45cef3c0a33f401c30ce3ee432076574f3e8f42c93ef"),
}

PARENT_ACCESSES = {
    0x515C8C: "load", 0x515D46: "load", 0x515D4C: "store",
    0x515DE2: "load", 0x5161BC: "load", 0x5168EE: "store",
    0x5169E2: "store", 0x516E10: "load", 0x516E1A: "load",
    0x539354: "load", 0x5399CE: "load", 0x539A46: "load",
}

TOUCH_THUNKS = {
    0x200: "_ZThn224_N7cocos2d7CCLayer12ccTouchBeganEPNS_7CCTouchEPNS_7CCEventE",
    0x204: "_ZThn224_N7cocos2d7CCLayer12ccTouchMovedEPNS_7CCTouchEPNS_7CCEventE",
    0x208: "_ZThn224_N7cocos2d7CCLayer12ccTouchEndedEPNS_7CCTouchEPNS_7CCEventE",
    0x20C: "_ZThn224_N7cocos2d7CCLayer16ccTouchCancelledEPNS_7CCTouchEPNS_7CCEventE",
    0x210: "_ZThn224_N7cocos2d7CCLayer14ccTouchesBeganEPNS_5CCSetEPNS_7CCEventE",
    0x214: "_ZThn224_N7cocos2d7CCLayer14ccTouchesMovedEPNS_5CCSetEPNS_7CCEventE",
    0x218: "_ZThn224_N7cocos2d7CCLayer14ccTouchesEndedEPNS_5CCSetEPNS_7CCEventE",
    0x21C: "_ZThn224_N7cocos2d7CCLayer18ccTouchesCancelledEPNS_5CCSetEPNS_7CCEventE",
}


def verify_function_hashes(elf: OriginalElf) -> None:
    for name, (address, size, digest) in FUNCTION_HASHES.items():
        elf.symbol(name, address, size)
        require(hashlib.sha256(elf.at(address, size)).hexdigest() == digest,
                "native function changed: " + name)


def function_names_at(elf: OriginalElf, address: int) -> list[str]:
    return sorted(name for start, size, name in elf.functions
                  if start == address and size > 0)


def verify_parent_access_inventory(elf: OriginalElf) -> list[dict]:
    prefixes = ("_ZN7cocos2d6CCNode", "_ZNK7cocos2d6CCNode",
                "_ZN7cocos2d7CCLayer", "_ZNK7cocos2d7CCLayer",
                "_ZN7cocos2d8CCSprite", "_ZNK7cocos2d8CCSprite")
    observed: dict[int, str] = {}
    owners: dict[int, set[str]] = {}
    visited = set()
    for start, size, name in elf.functions:
        if not name.startswith(prefixes):
            continue
        key = (start, size)
        if key in visited:
            continue
        visited.add(key)
        for pc in range(start, start + size - 3, 2):
            first, second = struct.unpack("<HH", elf.at(pc, 4))
            opcode = first & 0xFFF0
            if (second & 0xFFF) != 0xA8 or opcode not in (0xF8D0, 0xF8C0):
                continue
            kind = "load" if opcode == 0xF8D0 else "store"
            require(pc not in observed or observed[pc] == kind,
                    "conflicting parent access decode")
            observed[pc] = kind
            owners.setdefault(pc, set()).add(name)
    require(observed == PARENT_ACCESSES, "CCNode/CCLayer/CCSprite parent access inventory changed")
    return [{
        "address": f"0x{pc:08x}", "access": kind,
        "functions": sorted(owners.get(pc, set())),
    } for pc, kind in sorted(observed.items())]


def verify_vtable_slot(elf: OriginalElf, vtable_name: str, vtable_address: int,
                       vtable_size: int, slot: int, target_name: str,
                       target_address: int) -> None:
    value, size = elf.symbol(vtable_name, vtable_address, vtable_size)
    require((value & ~1) == vtable_address and size == vtable_size, "vtable identity mismatch")
    entry = vtable_address + 8 + slot
    require(elf.relative_relocations.get(entry) == 23, "vtable slot relocation mismatch")
    target, _ = elf.symbol(target_name, target_address)
    require(elf.u32(entry) == target, "vtable slot target mismatch")


def verify_parent_backrefs(elf: OriginalElf) -> dict:
    # showAToJTip(): CCSprite::create -> Controls::addChild(sprite)
    require(hashlib.sha256(elf.at(0x297938, 0xB0)).hexdigest() ==
            "bb9e8e86f49254d3a4584e766bf08df68900698593a7974b33e70e873a817d8d",
            "showAToJTip child-add window changed")
    # Controls::init(): CCLayer::create, retain at Controls+0x178, addChild(layer)
    require(elf.at(0x297EE0, 20).hex() ==
            "f28df9ca68224601c4f878014620d2f8b8209047",
            "Controls::init layer child-add window changed")

    verify_vtable_slot(elf, "_ZTVN7cocos2d8CCSpriteE", 0x9263D0, 632, 0xC8,
                       "_ZN7cocos2d6CCNode9setParentEPS0_", 0x515D4C)
    verify_vtable_slot(elf, "_ZTVN7cocos2d7CCLayerE", 0x91CF50, 560, 0xC8,
                       "_ZN7cocos2d6CCNode9setParentEPS0_", 0x515D4C)
    require(elf.at(0x515D4C, 6).hex() == "c0f8a8107047",
            "CCNode::setParent no longer stores parent at +0xa8")

    accesses = verify_parent_access_inventory(elf)
    return {
        "retained_children": [
            {"source": "Controls::showAToJTip", "class": "cocos2d::CCSprite",
             "parent_field": "CCNode+0xa8", "controls_storage": "temporary child"},
            {"source": "Controls::init", "class": "cocos2d::CCLayer",
             "parent_field": "CCNode+0xa8", "controls_storage": "Controls+0x178"},
        ],
        "parent_field_direct_access_inventory": accesses,
        "parent_backref_established": True,
        "parent_write_through_to_controls_pointer_fields_excluded": False,
    }


def immediate_stores(elf: OriginalElf, start: int, size: int) -> list[tuple[int, int]]:
    result = []
    for pc in range(start, start + size - 3, 2):
        first, second = struct.unpack("<HH", elf.at(pc, 4))
        if (first & 0xFFF0) in (0xF8C0, 0xF880):
            result.append((pc, second & 0xFFF))
    return result


def verify_touch_dispatch_backref(elf: OriginalElf) -> dict:
    # Controls constructor replaces CCLayer vptrs, including CCTouchDelegate at +0xe0.
    require(elf.at(0x29697C, 0x30).hex() ==
            "234b04f5b4707b441b6803f10802226003f5fc72c4f8e02003f50a72c4f8e42003f50d7203f511731a60c4f820e804f5",
            "Controls secondary vptr installation changed")
    controls_vtable, size = elf.symbol("_ZTV8Controls", 0x8E4490, 592)
    require(size == 592, "Controls vtable size changed")
    require(elf.u32(0x296A0C) + 0x296984 == 0x92EE04,
            "Controls constructor vtable GOT calculation changed")
    require(elf.relative_relocations.get(0x92EE04) == 23 and elf.u32(0x92EE04) == controls_vtable,
            "Controls vtable GOT mismatch")
    require(elf.u32(0x8E4490 + 8 + 0x19C) ==
            elf.symbol("_ZN7cocos2d7CCLayer27registerWithTouchDispatcherEv", 0x5248F2)[0],
            "Controls registerWithTouchDispatcher vtable slot changed")
    require(elf.u32(0x8E4490 + 8 + 0x1AC) ==
            elf.symbol("_ZN7cocos2d7CCLayer15setTouchEnabledEb", 0x524DA8)[0],
            "Controls setTouchEnabled vtable slot changed")

    # Controls::init sets touch enabled through slot 0x1ac.
    require(elf.at(0x297E6A, 12).hex() == "236820462101d3f8ac319847",
            "Controls::init setTouchEnabled call changed")
    # CCLayer constructor initializes script handler/mode/priority to zero. No
    # Controls method directly rewrites +0xf0/+0xfc/+0x100, so default mode is standard.
    require(elf.at(0x525246, 20).hex() ==
            "c4f8300fc4f8340fc4f8380fc4f83c0fc4f80031",
            "CCLayer touch registration defaults changed")
    forbidden = {0xF0, 0xFC, 0x100}
    controls_touch_config_writes = []
    for start, fsize, name in elf.functions:
        if not (name.startswith("_ZN8Controls") or name.startswith("_ZNK8Controls")):
            continue
        for pc in range(start, start + fsize - 3, 2):
            first, second = struct.unpack("<HH", elf.at(pc, 4))
            if (first & 0xFFF0) in (0xF8C0, 0xF880) and (second & 0xFFF) in forbidden:
                controls_touch_config_writes.append((pc, second & 0xFFF, name))
    require(not controls_touch_config_writes, "Controls rewrites CCLayer touch mode/script configuration")

    # When entering the scene, enabled CCLayer invokes virtual registration.
    require(elf.at(0x525B32, 18).hex() ==
            "f89430ec4605b12323682046d3f89c319847",
            "CCLayer::onEnter touch registration path changed")
    # registerWithTouchDispatcher passes the CCTouchDelegate subobject at +0xe0;
    # default +0x100==0 path tails to addStandardDelegate.
    require(elf.at(0x5248FA, 46).hex() ==
            "0368db6c9847d4f8f03004f1e0013bb11a7f0ab1002206e0d4f80021",
            "CCLayer standard touch delegate selection changed")
    require(elf.at(0x524920, 8).hex() == "bde8104024f066bd",
            "CCLayer addStandardDelegate tail-call changed")
    # CCTouchHandler retains delegate at +0x14 and calls retain().
    require(elf.at(0x549BC4, 38).hex() ==
            "38b5044641611546084649b1084901f06f084a794409687a441268b4f1f4fe",
            "CCTouchHandler delegate retention prefix changed")
    require(elf.at(0x549BE4, 12).hex() == "cff775fe0023a5610120",
            "CCTouchHandler delegate retain/store tail changed")

    # Controls touch-delegate secondary vptr points at inherited CCLayer thunks.
    require(elf.u32(0x8E4490 + 0x1F0) == 0xFFFFFF20,
            "Controls CCTouchDelegate this-adjustment changed")
    for table_offset, thunk_name in TOUCH_THUNKS.items():
        target, _ = elf.symbol(thunk_name)
        entry = 0x8E4490 + table_offset
        require(elf.relative_relocations.get(entry) == 23, "touch thunk vtable relocation mismatch")
        require(elf.u32(entry) == target, "Controls touch thunk vtable entry mismatch")
        require(elf.at(target & ~1, 4).hex() == "a0f1e000",
                "CCLayer touch thunk no longer subtracts 0xe0")

    callback_writes = {}
    for name in (
        "_ZN7cocos2d7CCLayer14ccTouchesBeganEPNS_5CCSetEPNS_7CCEventE",
        "_ZN7cocos2d7CCLayer14ccTouchesMovedEPNS_5CCSetEPNS_7CCEventE",
        "_ZN7cocos2d7CCLayer14ccTouchesEndedEPNS_5CCSetEPNS_7CCEventE",
        "_ZN7cocos2d7CCLayer18ccTouchesCancelledEPNS_5CCSetEPNS_7CCEventE",
    ):
        address, size, _ = FUNCTION_HASHES[name]
        stores = immediate_stores(elf, address, size)
        require(not [x for x in stores if x[1] in DANGEROUS],
                "inherited touch callback writes Controls pointer field")
        callback_writes[name] = [hex(offset) for _, offset in stores]

    # The inherited callbacks gate into the script engine through Controls+0xdc.
    require(elf.at(0x525350, 12).hex() == "d0f8dc300a4613b10021",
            "CCLayer multi-touch script gate changed")
    require(elf.at(0x516A48, 18).hex() ==
            "20f048fa006810b103689b689847c4f8dc00",
            "CCNode script-engine field initialization changed")

    return {
        "delegate_subobject": "Controls+0xe0",
        "retained_handler_field": "CCTouchHandler+0x14",
        "registration_mode": "standard_delegate",
        "controls_touch_vtable_inherits_cclayer": True,
        "inherited_touch_callback_direct_stores": callback_writes,
        "script_engine_gate_field": "Controls+0xdc",
        "native_touch_callbacks_write_controls_pointer_fields": False,
        "script_engine_write_through_excluded": False,
    }


def analyze(data: bytes) -> dict:
    require(hashlib.sha256(data).hexdigest() == SHA, "ELF SHA mismatch")
    elf = OriginalElf(data)
    verify_function_hashes(elf)
    parent = verify_parent_backrefs(elf)
    touch = verify_touch_dispatch_backref(elf)
    return {
        "format": FORMAT,
        "elf_sha256": SHA,
        "parent_backref": parent,
        "touch_dispatch_backref": touch,
        "known_cocos_retained_channels_checked": 2,
        "local_cocos_backrefs_characterized": True,
        "parent_write_through_to_controls_pointer_fields_excluded": False,
        "script_engine_write_through_excluded": False,
        "whole_program_alias_writes_excluded": False,
        "receiver_class_proven": False,
        "affects_routing": False,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original_apk_or_elf", type=Path)
    parser.add_argument("--details-output", type=Path, required=True)
    args = parser.parse_args()
    if zipfile.is_zipfile(args.original_apk_or_elf):
        with zipfile.ZipFile(args.original_apk_or_elf) as apk:
            data = apk.read("lib/armeabi-v7a/libcocos2dcpp.so")
    else:
        data = args.original_apk_or_elf.read_bytes()
    report = analyze(data)
    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    args.details_output.write_text(json.dumps(report, sort_keys=True) + "\n", encoding="utf-8")
    print("format:", FORMAT)
    print("parent direct access sites:", len(report["parent_backref"]["parent_field_direct_access_inventory"]))
    print("touch registration mode:", report["touch_dispatch_backref"]["registration_mode"])
    print("retained touch delegate:", report["touch_dispatch_backref"]["delegate_subobject"], "->", report["touch_dispatch_backref"]["retained_handler_field"])
    print("open hazards: parent write-through, script-engine write-through")
    print("whole-program alias exclusion: no; affects routing: no")
    print("details:", args.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
