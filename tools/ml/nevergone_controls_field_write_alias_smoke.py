#!/usr/bin/env python3
"""Synthetic regression gates for exact Controls method/alias-write coverage."""
from __future__ import annotations

import copy

from audit_nevergone_controls_field_native_store_inventory import is_controls
from audit_nevergone_controls_field_write_aliases import (
    audit, FACTORY, ELF_SHA, GHIDRA_FORMAT, NATIVE_FORMAT,
)


def fixtures():
    entries = [f"{0x2a0000 + i * 0x10:08x}" for i in range(38)]
    native = [{
        "format": NATIVE_FORMAT, "record_type": "summary",
        "elf_sha256": ELF_SHA, "controls_unique_method_count": 38,
        "controls_symbol_count": 40,
        "controls_method_entries": entries,
        "controls_store_addresses": sorted(FACTORY),
        "whole_elf_store_encoding_candidates": 3,
        "unowned_encoding_candidates": 0,
    }]
    for addr, field in FACTORY.items():
        native.append({
            "format": NATIVE_FORMAT, "record_type": "store_encoding_candidate",
            "ghidra_address": addr, "offset": field, "base_register": "r4",
            "value_register": "r0", "controls_method": True,
        })
    manifest = [{"function_entry": e, "elf_entry": f"{int(e, 16) - 0x10000:08x}",
                 "symbol_names": "_ZN8Controls4initEv"} for e in entries]
    ghidra = [{
        "format": GHIDRA_FORMAT, "row_type": "summary", "function_entry": e,
        "decompile_status": "ok", "store_count": "3" if i == 0 else "0",
        "call_count": "1" if i == 0 else "0",
        "unknown_store_count": "0", "this_escape_count": "1" if i == 0 else "0",
    } for i, e in enumerate(entries)]
    for addr, field in FACTORY.items():
        ghidra.append({
            "format": GHIDRA_FORMAT, "row_type": "store", "function_entry": entries[0],
            "op_address": addr, "op_time": "0", "decompile_status": "ok",
            "address_kind": "base_plus_constant", "address_offset": field,
            "base_parameter_index": "0", "address_base_token": "register:register:00000000:4",
        })
    ghidra.append({
        "format": GHIDRA_FORMAT, "row_type": "call", "function_entry": entries[0],
        "op_address": "002a8124", "op_time": "1", "decompile_status": "ok",
        "call_kind": "CALL", "this_argument_indices": "0",
    })
    return native, manifest, ghidra


def expect_incomplete(native, manifest, ghidra, expected):
    result = audit(native, manifest, ghidra)
    assert result["status"] == "bounded_controls_method_inventory_incomplete", result
    assert any(expected in reason for reason in result["reasons"]), result


def main():
    assert is_controls("_ZN8Controls4initEv")
    assert is_controls("_ZNK8Controls12hideControlsEv")
    assert not is_controls("_ZN10CCJoyStick4initEv")
    assert not is_controls("_ZN14ControlsButtonC2Ev")

    native, manifest, ghidra = fixtures()
    result = audit(native, manifest, ghidra)
    assert result["status"] == "bounded_controls_methods_scanned_with_alias_caveat", result
    assert result["controls_methods_decompiled"] == 38
    assert result["ghidra_direct_this_field_writers"] == sorted(FACTORY)
    assert result["ghidra_calls_passing_this"] == 1
    assert result["all_alias_writers_excluded"] is False
    assert result["receiver_class_proven"] is False
    assert result["affects_routing"] is False

    bad = copy.deepcopy(ghidra)
    bad.pop(1)
    expect_incomplete(native, manifest, bad, "summary_not_unique")
    bad = copy.deepcopy(ghidra)
    bad[0]["decompile_status"] = "decompile_failed"
    expect_incomplete(native, manifest, bad, "method_not_decompiled")
    bad = copy.deepcopy(ghidra)
    bad[0]["store_count"] = "2"
    expect_incomplete(native, manifest, bad, "method_operation_counts_mismatch")
    bad = copy.deepcopy(ghidra)
    bad[38]["address_offset"] = "0x130"
    expect_incomplete(native, manifest, bad, "factory_field_offset_mismatch")
    bad = copy.deepcopy(ghidra)
    bad[38]["base_parameter_index"] = "-"
    bad[0]["unknown_store_count"] = "1"
    expect_incomplete(native, manifest, bad, "direct_this_field_writers_differ")
    bad = copy.deepcopy(ghidra)
    bad[0]["this_escape_count"] = "0"
    expect_incomplete(native, manifest, bad, "method_escape_count_mismatch")
    bad = copy.deepcopy(ghidra)
    bad[0]["unknown_store_count"] = "1"
    expect_incomplete(native, manifest, bad, "method_unknown_store_count_mismatch")

    bad_manifest = copy.deepcopy(manifest)
    bad_manifest[0]["function_entry"] = "00000000"
    try:
        audit(native, bad_manifest, ghidra)
    except ValueError:
        pass
    else:
        raise AssertionError("untrusted method manifest accepted")

    bad_native = copy.deepcopy(native)
    bad_native[1]["offset"] = "0x134"
    try:
        audit(bad_native, manifest, ghidra)
    except ValueError:
        pass
    else:
        raise AssertionError("wrong native field store accepted")

    print("Controls field write alias audit smoke: ok")


if __name__ == "__main__":
    main()
