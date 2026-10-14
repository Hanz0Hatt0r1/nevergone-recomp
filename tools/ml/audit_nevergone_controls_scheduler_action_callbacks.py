#!/usr/bin/env python3
"""Verify retained Controls* scheduler/action callbacks in the exact ARM ELF.

This closes the three non-ControlsButton callback mechanisms remaining after
native 78-call offset recovery: two scheduleOnce calls, one persistent
schedule call, and one CCCallFuncN action callback factory.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import zipfile
from pathlib import Path

from audit_nevergone_controls_retained_alias_closure import (
    OriginalElf,
    immediate_stores,
    require,
)

FORMAT = "NEVERGONE.ControlsSchedulerActionCallbackClosure/1"
DANGEROUS = {0x124, 0x12C, 0x130, 0x134, 0x174}
REMOVE_CHILD = "_ZN7cocos2d6CCNode11removeChildEPS0_"
REMOVE_CHILD_CLEANUP = "_ZN7cocos2d6CCNode11removeChildEPS0_b"
DETACH_CHILD = "_ZN7cocos2d6CCNode11detachChildEPS0_b"
SHOW_KILL_TIPS_CALLBACK_BYTES = bytes.fromhex("08b50368d3f8d830984708bd")
CALLBACKS = {
    "_ZN8Controls8comboEndEf": (
        0x296790,
        24,
        "cc7eaf11f4c594800e02f991e42d958c5c3de08a49b41d6cc1b5fe6adf9c930e",
    ),
    "_ZN8Controls4tickEf": (
        0x2967A8,
        460,
        "d2a1efe2acca8cc6a8dfc8273233cb8023059f99cdf0a9d066f706e90f566ae4",
    ),
    "_ZN8Controls20showKillTipsCallbackEPN7cocos2d6CCNodeE": (
        0x2966D4,
        12,
        "4db47efeaf39b353861afb29a9cd5a9e00aebec0aad339db8cc0251ef31b5259",
    ),
}
GOT = {
    0x92EE08: "_ZN8Controls8comboEndEf",
    0x92EE0C: "_ZN8Controls20showKillTipsCallbackEPN7cocos2d6CCNodeE",
    0x92EE1C: "_ZN8Controls4tickEf",
}
WINDOWS = {
    "DownDownA_scheduleOnce": (0x29772C, 0x29774A, "892a984e0c8f14cacdb1fc766ccd993943a146dd4e4d2b4e0738da6271cd5712"),
    "DownDownB_scheduleOnce": (0x29776C, 0x29778A, "2f18f3ac5e075d4c003a9d797bb9995b4691d6bbff7a8023a86afc4323b3ca5b"),
    "init_schedule": (0x29829E, 0x2982B6, "5894602c82ebf61a77b508ddf7ea1ff371e598ea6453abfe61ffcca0d63f7cb8"),
    "showAToJTip_CCCallFuncN": (0x2979B0, 0x2979C6, "3c6cef315969041f06c83dbdcaca8b158b1738f6d450fea89be7731bdf676c92"),
    "CCNode_schedule_core": (0x516F12, 0x516F44, "859d643d0cd4107d9f5e67e64bbf3a7be840e615cbff311614b1961e8a87feb5"),
    "CCNode_schedule": (0x516F44, 0x516F62, "aa4f0a61d01956a01268c551a891606818ecddc221754ba9f50c208cd37aea2c"),
    "CCNode_scheduleOnce": (0x516F80, 0x516F9C, "50494c0ac5875b490bfa6f623f444b0e2608c4508178c55d534f97660e2bd9a8"),
    "CCCallFunc_initWithTarget": (0x50EA1C, 0x50EA38, "5fe49dae178eb364b43c0fb3f33f9a22eb908565be0f9b09a630426a278b3162"),
    "CCCallFuncN_initWithTarget": (0x50EA38, 0x50EA58, "793b607996e1c297342cd312807d1e5524774cb5925dd1a6b4e498c372e5f1d3"),
    "CCCallFuncN_execute": (0x50ED12, 0x50ED4E, "57e78355b3beb65c90812adebdb52b16d08054596ff11665935b293231c64129"),
    "CCNode_removeChild_cleanup": (0x516C1E, 0x516C42, "23274966faa32352b2b3b5792e65cc0182b38084c80c7981319f6705908471fa"),
}


def verify_window(elf: OriginalElf, name: str) -> None:
    start, end, digest = WINDOWS[name]
    require(hashlib.sha256(elf.at(start, end - start)).hexdigest() == digest,
            "native callback window changed: " + name)


def verify_callback_symbols_and_writes(elf: OriginalElf) -> dict:
    result = {}
    for name, (address, size, digest) in CALLBACKS.items():
        elf.symbol(name, address, size)
        body = elf.at(address, size)
        require(hashlib.sha256(body).hexdigest() == digest, "callback body changed: " + name)
        stores = immediate_stores(elf, address, size)
        dangerous = [row for row in stores if row[4] in DANGEROUS]
        require(not dangerous, "callback directly writes a retained pointer field: " + name)
        result[name] = [hex(row[4]) for row in stores]
    return result


def verify_got(elf: OriginalElf) -> dict:
    result = {}
    for got, name in GOT.items():
        value, _size = elf.symbol(name, *CALLBACKS[name][:2])
        require(elf.relative_relocations.get(got) == 23, "callback GOT relocation mismatch")
        require(elf.u32(got) == value, "callback GOT value mismatch")
        result[hex(got)] = name
    return result


def verify_scheduler_path(elf: OriginalElf) -> dict:
    for name in ("DownDownA_scheduleOnce", "DownDownB_scheduleOnce", "init_schedule",
                 "CCNode_schedule_core", "CCNode_schedule", "CCNode_scheduleOnce"):
        verify_window(elf, name)

    # Exact member-function setup: DownDownA/B use GOT 0x92ee08 -> comboEnd,
    # while Controls::init uses GOT 0x92ee1c -> tick. The wrappers preserve
    # the member pointer and the core forwards r3 = node/target to CCScheduler.
    require(elf.at(0x297734, 2).hex() == "2046", "DownDownA target is not Controls this")
    require(elf.at(0x297774, 2).hex() == "2046", "DownDownB target is not Controls this")
    require(elf.at(0x2982A4, 2).hex() == "2046", "init schedule target is not Controls this")
    require(elf.at(0x516F32, 2).hex() == "2b46", "schedule core target forwarding changed")

    return {
        "scheduleOnce_calls": 2,
        "scheduleOnce_callback": "Controls::comboEnd(float)",
        "schedule_calls": 1,
        "schedule_callback": "Controls::tick(float)",
        "target_is_unadjusted_controls_this": True,
        "callback_targets_directly_write_pointer_fields": False,
    }


def verify_show_kill_tips_dispatch_identity(elf: OriginalElf) -> tuple[int, int]:
    # Exact callback body:
    #   push {r3,lr}; ldr r3,[r0]; ldr.w r3,[r3,#0xd8]; blx r3; pop {r3,pc}
    # r0 is therefore the retained Controls* target and slot 0xd8 is the
    # dispatch slot.  The previous audit incorrectly labeled this slot as
    # CCNode::getScheduler; exact Cocos 2.1.2 layout and the ELF vtable map it
    # to the one-argument CCNode::removeChild wrapper.
    require(elf.at(0x2966D4, 12) == SHOW_KILL_TIPS_CALLBACK_BYTES,
            "showKillTips callback receiver/slot body changed")
    vtable, size = elf.symbol("_ZTV8Controls", 0x8E4490, 592)
    entry = vtable + 8 + 0xD8
    require(entry + 4 <= vtable + size, "Controls slot 0xd8 outside vtable")
    require(elf.relative_relocations.get(entry) == 23,
            "Controls slot 0xd8 relocation mismatch")
    target = elf.u32(entry)
    remove_value, remove_size = elf.symbol(REMOVE_CHILD)
    require(target == remove_value,
            "showKillTips slot 0xd8 is not CCNode::removeChild(CCNode*)")
    return remove_value, remove_size


def verify_show_kill_tips_remove_child_path(elf: OriginalElf) -> dict:
    remove_value, remove_size = verify_show_kill_tips_dispatch_identity(elf)
    remove_start = remove_value & ~1
    remove_stores = immediate_stores(elf, remove_start, remove_size)
    require(not ({row[4] for row in remove_stores} & DANGEROUS),
            "CCNode::removeChild(CCNode*) directly writes a Controls pointer field")

    verify_window(elf, "CCNode_removeChild_cleanup")
    cleanup_value, cleanup_size = elf.symbol(REMOVE_CHILD_CLEANUP, 0x516C1E, 36)
    cleanup_stores = immediate_stores(elf, cleanup_value & ~1, cleanup_size)
    require(not ({row[4] for row in cleanup_stores} & DANGEROUS),
            "CCNode::removeChild(CCNode*,bool) directly writes a Controls pointer field")

    # Exact upstream semantics for this pinned Cocos build route the bool
    # overload through detachChild when the child is present.  Inspect the
    # exact packaged detachChild body as an additional direct-STORE guard.
    detach_value, detach_size = elf.symbol(DETACH_CHILD)
    detach_stores = immediate_stores(elf, detach_value & ~1, detach_size)
    require(not ({row[4] for row in detach_stores} & DANGEROUS),
            "CCNode::detachChild directly writes a Controls pointer field")

    return {
        "slot": "0xd8",
        "target": "cocos2d::CCNode::removeChild(CCNode*)",
        "target_symbol": REMOVE_CHILD,
        "target_direct_store_offsets": [hex(row[4]) for row in remove_stores],
        "cleanup_overload_symbol": REMOVE_CHILD_CLEANUP,
        "cleanup_direct_store_offsets": [hex(row[4]) for row in cleanup_stores],
        "detach_symbol": DETACH_CHILD,
        "detach_direct_store_offsets": [hex(row[4]) for row in detach_stores],
        "directly_writes_controls_pointer_fields": False,
    }


def verify_callfuncn_path(elf: OriginalElf) -> dict:
    for name in ("showAToJTip_CCCallFuncN", "CCCallFunc_initWithTarget",
                 "CCCallFuncN_initWithTarget", "CCCallFuncN_execute"):
        verify_window(elf, name)

    # showAToJTip passes r6 (saved Controls this) as target. CCCallFunc retains
    # it at +0x24, CCCallFuncN stores the member-pointer pair at +0x2c/+0x30,
    # and execute reloads both before BLX.
    require(elf.at(0x2979C0, 2).hex() == "3046", "CCCallFuncN target is not Controls this")
    require(elf.at(0x50EA32, 2).hex() == "6c62", "CCCallFunc target +0x24 store changed")
    require(elf.at(0x50EA4A, 8).hex() == "05f12c0383e80300", "CCCallFuncN member pointer storage changed")
    require(elf.at(0x50ED20, 24).hex().endswith("a1699847"), "CCCallFuncN member dispatch changed")

    remove_path = verify_show_kill_tips_remove_child_path(elf)

    return {
        "factory": "cocos2d::CCCallFuncN::create",
        "retained_target_field": "CCCallFunc+0x24",
        "member_pointer_fields": ["CCCallFuncN+0x2c", "CCCallFuncN+0x30"],
        "callback": "Controls::showKillTipsCallback(CCNode*)",
        "callback_virtual_slot": "0xd8",
        "callback_virtual_target": "cocos2d::CCNode::removeChild(CCNode*)",
        "callback_remove_child_path": remove_path,
        "callback_directly_writes_pointer_fields": False,
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
    got = verify_got(elf)
    callback_stores = verify_callback_symbols_and_writes(elf)
    scheduler = verify_scheduler_path(elf)
    action = verify_callfuncn_path(elf)
    report = {
        "format": FORMAT,
        "elf_sha256": hashlib.sha256(data).hexdigest(),
        "got_callbacks": got,
        "callback_store_offsets": callback_stores,
        "scheduler_callback_path": scheduler,
        "callfuncn_callback_path": action,
        "retained_scheduler_action_paths_closed": True,
        "callbacks_directly_write_controls_pointer_fields": False,
        "whole_program_alias_writes_excluded": False,
        "receiver_class_proven": False,
        "affects_routing": False,
    }
    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    args.details_output.write_text(json.dumps(report, sort_keys=True) + "\n", encoding="utf-8")

    print("format:", FORMAT)
    print("scheduler callbacks: comboEnd x2, tick x1")
    print("action callback: showKillTipsCallback -> CCNode::removeChild(CCNode*)")
    print("callbacks directly write retained pointer fields: no")
    print("whole-program alias exclusion: no; affects routing: no")
    print("output:", args.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
