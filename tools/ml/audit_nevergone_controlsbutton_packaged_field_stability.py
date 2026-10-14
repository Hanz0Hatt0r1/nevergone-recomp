#!/usr/bin/env python3
"""Prove the three ControlsButton receiver fields are stable in the exact packaged APK.

This is intentionally scoped to normal code contained in the pinned APK. It does
NOT claim resistance to injected code, debugger writes, memory corruption, or
other out-of-model runtime tampering. The result may authorize the three v14
receiver recoveries only when every exact native/Ghidra closure below agrees.
"""
from __future__ import annotations

import argparse
import collections
import csv
import hashlib
import io
import json
import struct
import tarfile
import zipfile
from pathlib import Path
from typing import Any

import audit_nevergone_controls_alias_native_crosscheck as alias_native
import audit_nevergone_controls_cocos_backrefs as cocos_backrefs
import audit_nevergone_controls_dlsym_closure as dlsym_closure
import audit_nevergone_controls_parent_script_static_closure as parent_script
import audit_nevergone_controls_register_store_native as register_native
import audit_nevergone_controls_scheduler_action_callbacks as scheduler_callbacks
import audit_nevergone_controls_this_call_native_offsets as call_offsets
import audit_nevergone_controls_retained_alias_closure as retained_alias
from audit_nevergone_controlsbutton_field_receiver_witness import (
    OriginalElf as ReceiverElf,
    read_jsonl as receiver_read_jsonl,
    read_tsv as receiver_read_tsv,
    verify_binary as verify_receiver_binary,
    verify_slices as verify_receiver_slices,
)

FORMAT = "NEVERGONE.ControlsButtonPackagedFieldStability/1"
SHA = "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"
SOURCE = "libcocos2dcpp.so:002a7594"
VTABLE = "libcocos2dcpp.so:008f46f8"
TARGET = "libcocos2dcpp.so:00525d2e"
SLOT = "0x80"
DANGEROUS = {0x124, 0x12C, 0x130, 0x134, 0x174}
TARGET_CALLS = {
    3: ("002a7612", "0x134", "002a81c2"),
    4: ("002a7634", "0x130", "002a8172"),
    7: ("002a7686", "0x12c", "002a8120"),
}
FIELD_ARCHIVE_MEMBERS = {
    "native": "controls-field-native-store-inventory.jsonl",
    "manifest": "controls-field-native-methods.tsv",
    "ghidra": "evidence/controls-field-write-aliases.tsv",
    "audit": "controls-field-write-alias-audit.jsonl",
}
OPERAND_ARCHIVE_MEMBERS = {
    "triage": "controls-field-alias-triage.jsonl",
    "plan": "controls-field-next-probe-plan.tsv",
    "roots": "evidence/controls-alias-operand-roots.tsv",
    "audit": "controls-alias-operand-roots-audit.jsonl",
}
EXTERNAL_BASE_FUNCTIONS = {
    "_ZN7cocos2d6CCNode22unscheduleAllSelectorsEv":
        (0x516FC4, 10, "3f7ba58201f8b83e09be3cb691288a7ce4d69d4ff6a7d45b97cf09f90cf04b2c", []),
    "_ZN7cocos2d7CCLayerD2Ev":
        (0x5253F8, 88, "7575bb07b4c3cb1bf510a3be2915b28629863f4e02d0299b02c9e6159bb1f54c", [0xE0,0xE4,0xE8,0xE0]),
    "_ZN7cocos2d7CCLayerC2Ev":
        (0x525204, 116, "3f2e3e01249857ae7e4d3cf895d7368b74e33793b9df533de75533a3d0bf6fb9",
         [0xE0,0xE4,0xE8,0xEC,0xED,0xEE,0xF0,0xF4,0xF8,0xFC,0x100,0xD1]),
}
CONTROLS_LIFETIME_HASHES = {
    "_ZN8ControlsC1Ev": (0x296974, 156, "d222af794b9e592d77d1f3b4aa95a6f239ed6a023d8f2613645eacddd2e2f857"),
    "_ZN8ControlsD1Ev": (0x2966F8, 68, "282b6d6f62883b58cf2e1722e0e00efab8bfffbcac5aaf0a2e66018036db856e"),
    "_ZN8ControlsD0Ev": (0x296744, 18, "ce454f97f9f1f6196c82aa84b6a7fea08b93766607811a06544b59362f57e9dd"),
}
EXPECTED_BASE_CALLS = collections.Counter({
    "_ZN7cocos2d6CCNode22unscheduleAllSelectorsEv": 1,
    "_ZN7cocos2d7CCLayerD2Ev": 1,
    "_ZN7cocos2d7CCLayerC2Ev": 1,
    "_ZN7cocos2d6CCNode12scheduleOnceEMNS_8CCObjectEFvfEf": 2,
    "_ZN7cocos2d6CCNode8scheduleEMNS_8CCObjectEFvfE": 1,
})


def require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def jsonl_text(value: str) -> list[dict[str, Any]]:
    rows = [json.loads(line) for line in value.splitlines() if line.strip()]
    require(rows and all(isinstance(row, dict) for row in rows), "invalid JSONL")
    return rows


def tsv_text(value: str) -> list[dict[str, str]]:
    return list(csv.DictReader(io.StringIO(value), delimiter="\t"))


def read_archive(path: Path, members: dict[str, str]) -> dict[str, str]:
    with tarfile.open(path, "r:gz") as archive:
        regular = [member for member in archive.getmembers() if member.isfile()]
        names = [member.name for member in regular]
        require(len(names) == len(set(names)), "duplicate archive member")
        require(set(members.values()).issubset(names), "evidence archive missing member")
        require(all(member.size <= 20_000_000 for member in regular), "oversized archive member")
        return {
            key: archive.extractfile(archive.getmember(name)).read().decode("utf-8")
            for key, name in members.items()
        }


def immediate_store_offsets(elf: cocos_backrefs.Elf, address: int, size: int) -> list[int]:
    result = []
    for pc in range(address, address + size - 3, 2):
        first, second = struct.unpack("<HH", elf.at(pc, 4))
        if (first & 0xFFF0) in (0xF8C0, 0xF880):
            result.append(second & 0xFFF)
    return result


def verify_external_base_calls(elf: cocos_backrefs.Elf, native_offsets: list[dict]) -> dict:
    details = [row for row in native_offsets if row.get("record_type") == "call_argument"]
    base = [row for row in details if row.get("callee_category") == "cocos_base_method_candidate"]
    require(collections.Counter(row.get("callee_symbol") for row in base) == EXPECTED_BASE_CALLS,
            "external Cocos base call set changed")
    for row in base:
        require(row.get("controls_relative_offset") == "0x0" and
                row.get("argument_base_offset_proven") is True,
                "external base call is not exact Controls this")

    checked = {}
    for name, (address, size, digest, expected_offsets) in EXTERNAL_BASE_FUNCTIONS.items():
        elf.sym(name, address, size)
        require(hashlib.sha256(elf.at(address, size)).hexdigest() == digest,
                "external Cocos function changed: " + name)
        stores = immediate_store_offsets(elf, address, size)
        require(stores == expected_offsets, "external Cocos STORE inventory changed: " + name)
        require(not (set(stores) & DANGEROUS), "external Cocos function writes receiver fields: " + name)
        checked[name] = [hex(value) for value in stores]
    return {"exact_direct_base_functions_checked": checked,
            "schedule_calls_covered_by_scheduler_closure": 3}


def verify_live_controls_vptr(field_ghidra: list[dict], elf: cocos_backrefs.Elf) -> dict:
    for name, (address, size, digest) in CONTROLS_LIFETIME_HASHES.items():
        elf.sym(name, address, size)
        require(hashlib.sha256(elf.at(address, size)).hexdigest() == digest,
                "Controls lifetime function changed: " + name)
    stores = [row for row in field_ghidra if row.get("row_type") == "store" and
              row.get("base_parameter_index") == "0" and row.get("address_offset") == "0x0"]
    identity = sorted((row.get("function_entry"), row.get("op_address")) for row in stores)
    require(identity == [("002a66f8", "002a6706"), ("002a6974", "002a698a")],
            "Controls primary-vptr STORE inventory changed")
    # Exact constructor vptr/GOT and inherited touch-vtable identities are checked
    # independently by ControlsCocosBackrefs/1. The only later same-object vptr
    # store is destructor entry, after the live-object call frontier ends.
    return {"constructor_primary_vptr_store": "002a698a",
            "destructor_primary_vptr_store": "002a6706",
            "other_live_primary_vptr_stores": 0,
            "live_controls_primary_vptr_stable": True}


def analyze(original_apk: Path, field_alias_archive: Path, operand_archive: Path,
            v11_inference: Path, receiver_slices: Path) -> list[dict[str, Any]]:
    require(zipfile.is_zipfile(original_apk), "exact original APK required")
    with zipfile.ZipFile(original_apk) as apk:
        elf_data = apk.read("lib/armeabi-v7a/libcocos2dcpp.so")
    require(hashlib.sha256(elf_data).hexdigest() == SHA, "original ELF SHA mismatch")

    field_archive = read_archive(field_alias_archive, FIELD_ARCHIVE_MEMBERS)
    operand = read_archive(operand_archive, OPERAND_ARCHIVE_MEMBERS)
    field_audit = jsonl_text(field_archive["audit"])
    require(len(field_audit) == 1, "field alias audit row count changed")
    fa = field_audit[0]
    require(fa.get("format") == "NEVERGONE.ControlsFieldWriteAliasAudit/1" and
            fa.get("status") == "bounded_controls_methods_scanned_with_alias_caveat" and
            fa.get("controls_methods_decompiled") == 38 and fa.get("reasons") == [] and
            fa.get("ghidra_stores_in_controls_methods") == 167 and
            fa.get("ghidra_calls_in_controls_methods") == 293 and
            fa.get("ghidra_unknown_address_stores") == 46 and
            fa.get("ghidra_calls_passing_this") == 78 and
            fa.get("ghidra_direct_this_field_writers") == ["002a8120","002a8172","002a81c2"],
            "38-method Controls alias inventory changed")
    field_ghidra = tsv_text(field_archive["ghidra"])

    triage_rows = jsonl_text(operand["triage"])
    register_rows = register_native.analyze(triage_rows, elf_data)
    rs = register_rows[0]
    require(rs.get("direct_owner_store_count") == 37 and
            rs.get("unresolved_local_store_sites_after_native_crosschecks") == 0 and
            not (set(int(key, 16) for key in rs.get("owner_offsets", {})) & DANGEROUS),
            "37 direct Controls STORE closure changed")

    alias_rows = alias_native.analyze(operand, elf_data)
    als = alias_rows[0]
    require(als.get("site_count") == 15 and
            als.get("site_kinds") == {"factory":3,"indirect":3,"store":9},
            "15-site operand native closure changed")
    store_rows = [row for row in alias_rows[1:] if row.get("kind") == "store"]
    require(len(store_rows) == 9 and all(row.get("pointer_field_overwrite_proven") is False
                                         for row in store_rows),
            "child-object STORE closure changed")
    indirect_rows = [row for row in alias_rows[1:] if row.get("kind") == "indirect"]
    require(len(indirect_rows) == 3 and all(
        row.get("receiver_parameter_zero_proven_for_this_call") is True for row in indirect_rows),
        "Controls-this CALLIND receiver closure changed")

    native_offsets = call_offsets.analyze(triage_rows, elf_data)
    ns = native_offsets[0]
    require(ns.get("unadjusted_controls_this_calls") == 35 and
            ns.get("nonzero_controls_relative_calls") == 43 and
            ns.get("unresolved_offsets") == 0 and
            ns.get("all_ccpoint_calls_are_nonzero_subobjects") is True and
            ns.get("all_non_ccpoint_calls_are_unadjusted_controls_this") is True,
            "78-call Controls argument closure changed")

    retained_elf = retained_alias.OriginalElf(elf_data)
    button = retained_alias.verify_controlsbutton_callback_path(retained_elf)
    joystick = retained_alias.verify_joystick_delegate_path(retained_elf)
    require(button.get("pointer_field_write") is False and
            joystick.get("pointer_field_write") is False,
            "retained Controls alias callback writes receiver fields")

    got = scheduler_callbacks.verify_got(retained_elf)
    callback_stores = scheduler_callbacks.verify_callback_symbols_and_writes(retained_elf)
    scheduler = scheduler_callbacks.verify_scheduler_path(retained_elf)
    action = scheduler_callbacks.verify_callfuncn_path(retained_elf)
    require(scheduler.get("callback_targets_directly_write_pointer_fields") is False and
            action.get("callback_directly_writes_pointer_fields") is False,
            "scheduler/action callback writes receiver fields")

    cocos = cocos_backrefs.analyze(elf_data)
    require(cocos.get("local_cocos_backrefs_characterized") is True and
            cocos.get("known_cocos_retained_channels_checked") == 2 and
            cocos["touch_dispatch_backref"].get("native_touch_callbacks_write_controls_pointer_fields") is False,
            "Cocos retained back-reference characterization changed")
    parent = parent_script.analyze(original_apk)
    require(parent.get("known_parent_native_dangerous_write_through_excluded") is True and
            parent.get("packaged_static_script_engine_write_through_reachable") is False,
            "parent/script static closure changed")
    dynamic = dlsym_closure.analyze(original_apk)
    require(dynamic.get("packaged_dlsym_lookup_of_set_script_engine_excluded") is True and
            dynamic.get("packaged_cross_library_lookup_of_set_script_engine_excluded") is True,
            "packaged dlsym setter closure changed")

    cocos_elf = cocos_backrefs.Elf(elf_data)
    external = verify_external_base_calls(cocos_elf, native_offsets)
    live_vptr = verify_live_controls_vptr(field_ghidra, cocos_elf)

    receiver_elf = ReceiverElf(elf_data)
    verify_receiver_binary(receiver_elf)
    receiver_details = verify_receiver_slices(receiver_read_tsv(receiver_slices),
                                               receiver_read_jsonl(v11_inference))
    selected = {row["call_index"]: row for row in receiver_details
                if row.get("source_id") == SOURCE and row.get("call_index") in TARGET_CALLS}
    require(set(selected) == set(TARGET_CALLS), "three quarantined actual receivers missing")

    details = []
    for call, (call_address, field, store) in TARGET_CALLS.items():
        row = selected[call]
        require(row.get("call_address") == call_address and
                row.get("dispatch_slot_offset") == SLOT and
                row.get("receiver_field_offset") == field and
                row.get("field_store_address") == store and
                row.get("constructor_linked_class_candidate") == "ControlsButton" and
                row.get("constructor_linked_vtable_id") == VTABLE and
                row.get("constructor_linked_slot_target_id") == TARGET,
                f"call {call}: actual receiver/factory identity changed")
        details.append({
            "format": FORMAT, "record_type": "receiver_field", "source_id": SOURCE,
            "call_index": call, "call_address": call_address,
            "dispatch_slot_offset": SLOT, "receiver_field_offset": field,
            "factory_store_address": store, "receiver_class_name": "ControlsButton",
            "receiver_vtable_id": VTABLE, "slot_target_id": TARGET,
            "actual_nested_receiver_provenance": True,
            "packaged_field_stability_proven": True,
            "packaged_receiver_class_proven": True,
            "caller_argument_provenance_used": False,
            "whole_program_alias_writes_excluded": False,
            "external_injected_code_excluded": False,
            "affects_routing": False,
        })

    summary = {
        "format": FORMAT, "record_type": "summary", "elf_sha256": SHA,
        "scope": "exact_packaged_apk_normal_code_execution",
        "target_fields": ["0x12c","0x130","0x134"],
        "target_calls": [3,4,7],
        "controls_methods_decompiled": 38,
        "local_stores_classified": 46,
        "unadjusted_controls_calls_classified": 35,
        "embedded_ccpoint_calls_excluded_as_full_owner_aliases": 43,
        "retained_button_and_joystick_alias_paths_closed": True,
        "scheduler_action_callback_paths_closed": True,
        "cocos_parent_touch_backrefs_characterized": True,
        "parent_native_dangerous_write_through_excluded": True,
        "packaged_static_script_engine_setter_reachable": False,
        "packaged_dlsym_set_script_engine_lookup_excluded": True,
        "direct_external_base_call_closure": external,
        "live_controls_primary_vptr": live_vptr,
        "packaged_controlsbutton_field_stability_proven": True,
        "packaged_receiver_class_proven": True,
        "caller_argument_quarantine_preserved": True,
        "whole_program_alias_writes_excluded": False,
        "external_injected_code_excluded": False,
        "affects_routing": False,
    }
    return [summary, *details]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original_apk", type=Path)
    parser.add_argument("field_alias_evidence_archive", type=Path)
    parser.add_argument("operand_roots_archive", type=Path)
    parser.add_argument("v11_inference_jsonl", type=Path)
    parser.add_argument("mixed_receiver_slices_tsv", type=Path)
    parser.add_argument("--details-output", required=True, type=Path)
    args = parser.parse_args()
    rows = analyze(args.original_apk, args.field_alias_evidence_archive,
                   args.operand_roots_archive, args.v11_inference_jsonl,
                   args.mixed_receiver_slices_tsv)
    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    args.details_output.write_text("".join(json.dumps(row, sort_keys=True) + "\n" for row in rows),
                                   encoding="utf-8")
    summary = rows[0]
    print("format:", FORMAT)
    print("scope:", summary["scope"])
    print("actual receiver fields stable in packaged program: 3/3")
    print("local unknown STOREs classified:", summary["local_stores_classified"])
    print("full Controls call frontier classified:", summary["unadjusted_controls_calls_classified"])
    print("packaged field stability: proven")
    print("injected/external mutation excluded: no")
    print("affects routing: no")
    print("details:", args.details_output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
