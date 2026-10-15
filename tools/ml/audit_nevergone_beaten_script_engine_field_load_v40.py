#!/usr/bin/env python3
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
from pathlib import Path

FORMAT = "NEVERGONE.BeatenScriptEngineFieldLoadV40/1"
EXPORT_FORMAT = "NEVERGONE.GhidraAccessorFieldLoadConsumers/1"
V39_FORMAT = "NEVERGONE.BeatenScriptEngineManagerV39/1"
SHA = "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"


def read_json(path: Path) -> dict[str, object]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError(f"expected JSON object: {path}")
    return data


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    if not rows:
        raise ValueError(f"empty TSV: {path}")
    return rows


def intval(row: dict[str, str], key: str) -> int:
    try:
        return int(row[key])
    except Exception as exc:
        raise ValueError(f"invalid integer field {key}: {row.get(key)!r}") from exc


def audit(v39_path: Path, export_path: Path) -> dict[str, object]:
    v39 = read_json(v39_path)
    if v39.get("format") != V39_FORMAT:
        raise ValueError("unexpected v39 format")
    if v39.get("elf_sha256") != SHA:
        raise ValueError("v39 ELF hash changed")
    if v39.get("manager_class_nonnull_writer_frontier_closed") is not True:
        raise ValueError("v39 manager writer frontier is not closed")
    if v39.get("engine_field_offset") != "0x0":
        raise ValueError("v39 manager engine field moved")
    if v39.get("shared_manager_return_use_export_complete") is not True:
        raise ValueError("v39 sharedManager export incomplete")
    if v39.get("shared_manager_accessor_call_sites") != 33:
        raise ValueError("v39 sharedManager call-site count changed")
    if v39.get("shared_manager_unsupported_uses") != 33:
        raise ValueError("v39 sharedManager unsupported-use count changed")
    if v39.get("shared_manager_direct_member_uses") != 0:
        raise ValueError("v39 unexpectedly has direct member uses")
    if v39.get("shared_manager_unsupported_opcode_counts") != {"LOAD": 33}:
        raise ValueError("v39 sharedManager unsupported shape changed")
    if v39.get("receiver_promotions") != 0 or v39.get("affects_routing") is not False:
        raise ValueError("v39 routing contract changed")

    rows = read_tsv(export_path)
    if {row.get("format") for row in rows} != {EXPORT_FORMAT}:
        raise ValueError("unexpected v40 Ghidra export format")
    summaries = [row for row in rows if row.get("row_type") == "summary"]
    if len(summaries) != 1:
        raise ValueError("v40 export summary is not unique")
    summary = summaries[0]
    if "CCScriptEngineManager::sharedManager" not in summary.get("accessor_name", ""):
        raise ValueError("wrong accessor in v40 export")
    if summary.get("caller_functions_failed") != "0":
        raise ValueError("v40 caller decompilation incomplete")
    if summary.get("status") != "complete_field_load_export":
        raise ValueError("v40 field-load export incomplete")

    call_sites = intval(summary, "accessor_call_sites")
    field_load_count = intval(summary, "manager_field_loads")
    manager_nonfield = intval(summary, "manager_nonfield_uses")
    if call_sites != int(v39["shared_manager_accessor_call_sites"]):
        raise ValueError("v40 accessor call-site count disagrees with v39")
    if field_load_count != call_sites:
        raise ValueError("expected exactly one manager field load per sharedManager call")
    if manager_nonfield != 0:
        raise ValueError("sharedManager result has non-field uses in v40")

    field_rows = [row for row in rows if row.get("row_type") == "field_load"]
    if len(field_rows) != field_load_count:
        raise ValueError("field-load row count does not match summary")
    offsets = Counter(row.get("load_offset", "-") for row in field_rows)
    if offsets != {str(v39["engine_field_offset"]): field_load_count}:
        raise ValueError(f"sharedManager loads do not all target engine field: {dict(offsets)}")
    sites = {(row.get("caller_entry"), row.get("accessor_call_site")) for row in field_rows}
    if len(sites) != call_sites:
        raise ValueError("field loads are not one-to-one with accessor call sites")

    use_rows = [row for row in rows if row.get("row_type") == "loaded_use"]
    kinds = Counter(row.get("use_kind", "-") for row in use_rows)
    expected_summary = {
        "receiver_call": "loaded_value_receiver_calls",
        "nonreceiver_call": "loaded_value_nonreceiver_calls",
        "retention_store": "loaded_value_retention_stores",
        "return_escape": "loaded_value_return_escapes",
        "dereference_read": "loaded_value_dereference_reads",
        "dereference_write": "loaded_value_dereference_writes",
        "null_check": "loaded_value_null_checks",
        "opaque_use": "loaded_value_opaque_uses",
    }
    for kind, key in expected_summary.items():
        if intval(summary, key) != kinds.get(kind, 0):
            raise ValueError(f"summary count mismatch for {kind}")

    escape_count = (
        kinds.get("nonreceiver_call", 0)
        + kinds.get("retention_store", 0)
        + kinds.get("return_escape", 0)
        + kinds.get("opaque_use", 0)
    )
    direct_write_count = kinds.get("dereference_write", 0)
    engine_pointer_escape_frontier_closed = escape_count == 0
    engine_pointer_direct_write_frontier_closed = direct_write_count == 0

    residual = ["external_or_injected_runtime_code"]
    if not engine_pointer_escape_frontier_closed:
        residual.append("loaded_engine_pointer_escape_or_opaque_use")
    if not engine_pointer_direct_write_frontier_closed:
        residual.append("loaded_engine_pointer_direct_memory_write")
    residual.append("singleton_backing_cell_direct_access_not_yet_bounded")

    return {
        "format": FORMAT,
        "elf_sha256": SHA,
        "manager_class": "cocos2d::CCScriptEngineManager",
        "engine_field_offset": str(v39["engine_field_offset"]),
        "shared_manager_accessor_call_sites": call_sites,
        "shared_manager_engine_field_load_count": field_load_count,
        "shared_manager_engine_field_load_offsets": dict(sorted(offsets.items())),
        "shared_manager_engine_field_load_frontier_closed": True,
        "loaded_engine_pointer_receiver_call_count": kinds.get("receiver_call", 0),
        "loaded_engine_pointer_nonreceiver_call_count": kinds.get("nonreceiver_call", 0),
        "loaded_engine_pointer_retention_store_count": kinds.get("retention_store", 0),
        "loaded_engine_pointer_return_escape_count": kinds.get("return_escape", 0),
        "loaded_engine_pointer_dereference_read_count": kinds.get("dereference_read", 0),
        "loaded_engine_pointer_dereference_write_count": direct_write_count,
        "loaded_engine_pointer_null_check_count": kinds.get("null_check", 0),
        "loaded_engine_pointer_opaque_use_count": kinds.get("opaque_use", 0),
        "loaded_engine_pointer_escape_frontier_closed": engine_pointer_escape_frontier_closed,
        "loaded_engine_pointer_direct_write_frontier_closed": engine_pointer_direct_write_frontier_closed,
        "singleton_backing_cell_direct_access_bounded": False,
        "external_or_injected_runtime_code_excluded": False,
        "whole_program_alias_writes_to_manager_engine_slot_excluded": False,
        "engine_call_runtime_reachability_proven": False,
        "script_side_effects_closed": False,
        "residual_script_engine_frontier": residual,
        "receiver_promotions": 0,
        "affects_routing": False,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("v39_report", type=Path)
    parser.add_argument("field_load_export", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.v39_report, args.field_load_export)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"sharedManager engine-field loads: {result['shared_manager_engine_field_load_count']}")
    print(f"loaded engine receiver calls: {result['loaded_engine_pointer_receiver_call_count']}")
    print(f"loaded engine pointer escapes/opaque: {not result['loaded_engine_pointer_escape_frontier_closed']}")
    print("singleton backing cell bounded: no")
    print("receiver promotions: 0")
    print("affects routing: no")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
