#!/usr/bin/env python3
"""Audit exact Controls method coverage and possible field/alias writes.

A complete bounded method scan is NOT a whole-program alias-exclusion proof.
"""
from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Any

NATIVE_FORMAT = "NEVERGONE.ControlsFieldNativeStoreInventory/1"
GHIDRA_FORMAT = "NEVERGONE.GhidraControlsFieldWriteAliasInventory/1"
OUT_FORMAT = "NEVERGONE.ControlsFieldWriteAliasAudit/1"
FIELDS = {"0x12c", "0x130", "0x134"}
FACTORY = {
    "002a8120": "0x12c",
    "002a8172": "0x130",
    "002a81c2": "0x134",
}
ELF_SHA = "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"


def read_jsonl(path: Path) -> list[dict[str, Any]]:
    result = [json.loads(line) for line in path.read_text(encoding="utf-8").splitlines()
              if line.strip()]
    if not all(isinstance(row, dict) for row in result):
        raise ValueError("expected JSON objects")
    return result


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        return list(csv.DictReader(handle, delimiter="\t"))


def integer(value: str | None) -> int:
    try:
        return int(str(value))
    except (ValueError, TypeError):
        return -1


def audit(native: list[dict], manifest: list[dict], ghidra: list[dict]) -> dict:
    errors: list[str] = []
    if not native or native[0].get("format") != NATIVE_FORMAT or native[0].get("record_type") != "summary":
        raise ValueError("missing native inventory summary")
    ns = native[0]
    if (ns.get("elf_sha256") != ELF_SHA or
        ns.get("controls_unique_method_count") != 38 or
        ns.get("controls_symbol_count") != 40 or
        ns.get("controls_store_addresses") != sorted(FACTORY)):
        raise ValueError("stale or mismatched native inventory")
    if any(row.get("format") != NATIVE_FORMAT or
           row.get("record_type") != "store_encoding_candidate" for row in native[1:]):
        raise ValueError("native candidate format mismatch")
    if len(native) - 1 != ns.get("whole_elf_store_encoding_candidates"):
        raise ValueError("native candidate count mismatch")
    control_rows = [r for r in native[1:] if r.get("controls_method") is True]
    if {r.get("ghidra_address") for r in control_rows} != set(FACTORY):
        raise ValueError("native Controls store identity mismatch")
    if any(r.get("offset") != FACTORY[r["ghidra_address"]] or
           r.get("base_register") != "r4" or r.get("value_register") != "r0"
           for r in control_rows):
        raise ValueError("native Controls store operands mismatch")

    entries = [r.get("function_entry") for r in manifest]
    if (len(entries) != 38 or len(set(entries)) != 38 or
            sorted(entries) != ns.get("controls_method_entries") or
            any(not isinstance(e, str) or len(e) != 8 for e in entries)):
        raise ValueError("Controls method manifest mismatch")
    if any(row.get("format") != GHIDRA_FORMAT or row.get("function_entry") not in entries or
           row.get("row_type") not in {"summary", "store", "call"} for row in ghidra):
        raise ValueError("Ghidra row format or method identity mismatch")

    by_function: dict[str, list[dict]] = {
        entry: [row for row in ghidra if row["function_entry"] == entry]
        for entry in entries
    }
    all_stores: list[dict] = []
    all_calls: list[dict] = []
    failed_functions: list[str] = []
    for entry, rows in by_function.items():
        summaries = [r for r in rows if r["row_type"] == "summary"]
        stores = [r for r in rows if r["row_type"] == "store"]
        calls = [r for r in rows if r["row_type"] == "call"]
        if len(summaries) != 1:
            errors.append("summary_not_unique:" + entry)
            continue
        summary = summaries[0]
        if summary.get("decompile_status") != "ok":
            failed_functions.append(entry)
            errors.append("method_not_decompiled:" + entry)
            continue
        if (integer(summary.get("store_count")) != len(stores) or
                integer(summary.get("call_count")) != len(calls)):
            errors.append("method_operation_counts_mismatch:" + entry)
        unknown = sum(
            r.get("address_offset") == "-" or r.get("base_parameter_index") == "-" or
            str(r.get("address_kind", "")).startswith("opaque_")
            for r in stores
        )
        if integer(summary.get("unknown_store_count")) != unknown:
            errors.append("method_unknown_store_count_mismatch:" + entry)
        escapes = sum(r.get("this_argument_indices") not in ("-", "", None) for r in calls)
        if integer(summary.get("this_escape_count")) != escapes:
            errors.append("method_escape_count_mismatch:" + entry)
        if any(r.get("decompile_status") != "ok" or r.get("op_address") in (None, "", "-")
               or integer(r.get("op_time")) < 0 for r in [*stores, *calls]):
            errors.append("operation_identity_missing:" + entry)
        all_stores.extend(stores)
        all_calls.extend(calls)

    direct_this_writes = [r for r in all_stores
                          if r.get("base_parameter_index") == "0" and
                          r.get("address_offset") in FIELDS and
                          not str(r.get("address_kind", "")).startswith("opaque_")]
    direct_by_address = {r.get("op_address"): r for r in direct_this_writes}
    if len(direct_by_address) != len(direct_this_writes):
        errors.append("duplicate_direct_field_write_address")
    if set(direct_by_address) != set(FACTORY):
        errors.append("direct_this_field_writers_differ_from_native_witness")
    for address, offset in FACTORY.items():
        if address in direct_by_address and direct_by_address[address].get("address_offset") != offset:
            errors.append("factory_field_offset_mismatch:" + address)

    unknown_stores = [r for r in all_stores
                      if r.get("address_offset") == "-" or
                      r.get("base_parameter_index") == "-" or
                      str(r.get("address_kind", "")).startswith("opaque_")]
    this_calls = [r for r in all_calls if r.get("this_argument_indices") not in ("-", "", None)]
    status = ("bounded_controls_methods_scanned_with_alias_caveat" if not errors else
              "bounded_controls_method_inventory_incomplete")
    return {
        "format": OUT_FORMAT,
        "status": status,
        "controls_methods_expected": len(entries),
        "controls_methods_summarized": sum(
            len([r for r in rows if r["row_type"] == "summary"]) == 1
            for rows in by_function.values()
        ),
        "controls_methods_decompiled": len(entries) - len(failed_functions) -
            sum(len([r for r in rows if r["row_type"] == "summary"]) != 1
                for rows in by_function.values()),
        "failed_functions": failed_functions,
        "native_whole_elf_offset_encoding_candidates": len(native) - 1,
        "native_unowned_offset_encoding_candidates": ns.get("unowned_encoding_candidates"),
        "native_controls_direct_writers": sorted(FACTORY),
        "ghidra_stores_in_controls_methods": len(all_stores),
        "ghidra_calls_in_controls_methods": len(all_calls),
        "ghidra_direct_this_field_writers": sorted(direct_by_address),
        "ghidra_unknown_address_stores": len(unknown_stores),
        "ghidra_calls_passing_this": len(this_calls),
        "unknown_address_store_samples": [
            {"function_entry": r["function_entry"], "op_address": r["op_address"],
             "address_kind": r.get("address_kind"), "base": r.get("address_base_token")}
            for r in unknown_stores[:40]
        ],
        "this_escape_samples": [
            {"function_entry": r["function_entry"], "op_address": r["op_address"],
             "call_kind": r.get("call_kind"), "this_argument_indices": r.get("this_argument_indices")}
            for r in this_calls[:40]
        ],
        "reasons": sorted(set(errors)),
        "all_alias_writers_excluded": False,
        "receiver_class_proven": False,
        "affects_routing": False,
    }


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("native_inventory_jsonl", type=Path)
    p.add_argument("controls_method_manifest_tsv", type=Path)
    p.add_argument("ghidra_field_write_alias_tsv", type=Path)
    p.add_argument("--details-output", type=Path, required=True)
    p.add_argument("--require-complete", action="store_true")
    args = p.parse_args()
    detail = audit(read_jsonl(args.native_inventory_jsonl),
                   read_tsv(args.controls_method_manifest_tsv),
                   read_tsv(args.ghidra_field_write_alias_tsv))
    args.details_output.parent.mkdir(parents=True, exist_ok=True)
    args.details_output.write_text(json.dumps(detail, sort_keys=True) + "\n", encoding="utf-8")
    print("format:", OUT_FORMAT)
    print("status:", detail["status"])
    print("methods decompiled:", detail["controls_methods_decompiled"], "/",
          detail["controls_methods_expected"])
    print("direct Controls field writes:", detail["ghidra_direct_this_field_writers"])
    print("unknown address stores:", detail["ghidra_unknown_address_stores"])
    print("calls passing this:", detail["ghidra_calls_passing_this"])
    print("reasons:", detail["reasons"])
    print("alias exclusion: not proven; affects routing: no")
    print("output:", args.details_output)
    return 2 if args.require_complete and detail["reasons"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
