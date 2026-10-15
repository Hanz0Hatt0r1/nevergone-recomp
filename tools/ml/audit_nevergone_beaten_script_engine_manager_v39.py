#!/usr/bin/env python3
"""Inventory CCScriptEngineManager field writers and sharedManager return consumers after v38."""
from __future__ import annotations

import argparse
import csv
import json
from collections import Counter
from pathlib import Path

FORMAT = "NEVERGONE.BeatenScriptEngineManagerV39/1"
SHA = "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"
CLASS = "cocos2d::CCScriptEngineManager"
FIELD_FORMAT = "NEVERGONE.GhidraClassParameterFieldWrite/1"
FLOW_FORMAT = "NEVERGONE.GhidraClassOwnerAliasFlow/1"
CONSUMER_FORMAT = "NEVERGONE.GhidraAccessorReturnConsumers/1"


def read_json(path: Path) -> dict[str, object]:
    text = path.read_text(encoding="utf-8").strip()
    if not text:
        raise ValueError(f"empty JSON input: {path}")
    obj = json.loads(text.splitlines()[0]) if path.suffix == ".jsonl" else json.loads(text)
    if not isinstance(obj, dict):
        raise ValueError(f"expected JSON object: {path}")
    return obj


def read_tsv(path: Path) -> list[dict[str, str]]:
    with path.open(encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    if not rows:
        raise ValueError(f"empty TSV input: {path}")
    return rows


def parse_int(text: str) -> int:
    if text in ("", "-"):
        raise ValueError("missing integer")
    return int(text, 0)


def audit(v38_path: Path, field_path: Path, flow_path: Path, consumer_path: Path) -> dict[str, object]:
    v38 = read_json(v38_path)
    if v38.get("format") != "NEVERGONE.BeatenScriptEngineV38/1" or v38.get("elf_sha256") != SHA:
        raise ValueError("unexpected v38 contract")
    if v38.get("packaged_dynamic_lookup_of_set_script_engine_excluded") is not True:
        raise ValueError("v38 no longer excludes packaged dynamic setter lookup")
    if v38.get("external_or_injected_runtime_code_excluded") is not False:
        raise ValueError("v38 unexpectedly excludes external/runtime injection")
    if v38.get("whole_program_alias_writes_to_manager_engine_slot_excluded") is not False:
        raise ValueError("v38 unexpectedly closes manager alias writes")
    if v38.get("script_side_effects_closed") is not False or v38.get("receiver_promotions") != 0:
        raise ValueError("v38 routing/side-effect contract changed")

    fields = read_tsv(field_path)
    if {row.get("format") for row in fields} != {FIELD_FORMAT}:
        raise ValueError("unexpected manager field-write format")
    if {row.get("class_name") for row in fields} != {CLASS}:
        raise ValueError("manager field-write class changed")

    flows = read_tsv(flow_path)
    if {row.get("format") for row in flows} != {FLOW_FORMAT}:
        raise ValueError("unexpected manager owner-flow format")
    if {row.get("class_name") for row in flows} != {CLASS}:
        raise ValueError("manager owner-flow class changed")
    summaries = [row for row in flows if row.get("row_type") == "summary"]
    if not summaries:
        raise ValueError("manager owner-flow has no summaries")
    setter_summaries = [row for row in summaries if "setScriptEngine" in row.get("function_name", "")]
    if len(setter_summaries) != 1 or setter_summaries[0].get("owner_parameter_proven") != "1":
        raise ValueError("setScriptEngine owner parameter is not uniquely proven")

    setter_writes = [
        row for row in fields
        if "setScriptEngine" in row.get("function_name", "")
        and row.get("base_parameter_index") == "0"
        and row.get("value_root_parameter_index") == "1"
    ]
    if len(setter_writes) != 1:
        raise ValueError(f"expected one setScriptEngine param1 field write, got {len(setter_writes)}")
    setter = setter_writes[0]
    engine_offset = setter.get("address_offset", "-")
    engine_offset_int = parse_int(engine_offset)
    if engine_offset_int < 0 or engine_offset_int > 0x40 or engine_offset_int % 4:
        raise ValueError(f"implausible manager engine field offset: {engine_offset}")

    engine_writes = [row for row in fields if row.get("address_offset") == engine_offset]
    nonsetter_engine_writes = [row for row in engine_writes if row is not setter]
    safe_zero_writes = [
        row for row in nonsetter_engine_writes
        if row.get("value_root_token", "").startswith("const:0x0:")
    ]
    unsafe_engine_writes = [row for row in nonsetter_engine_writes if row not in safe_zero_writes]

    consumers = read_tsv(consumer_path)
    if {row.get("format") for row in consumers} != {CONSUMER_FORMAT}:
        raise ValueError("unexpected sharedManager consumer format")
    summary = [row for row in consumers if row.get("row_type") == "summary"]
    if len(summary) != 1:
        raise ValueError("sharedManager consumer summary is not unique")
    summary_row = summary[0]
    if "sharedManager" not in summary_row.get("accessor_name", ""):
        raise ValueError("wrong accessor in consumer export")
    if summary_row.get("caller_functions_failed") != "0":
        raise ValueError("sharedManager direct caller decompilation incomplete")
    if summary_row.get("status") != "complete_return_use_export":
        raise ValueError("sharedManager return-use export incomplete")

    use_rows = [row for row in consumers if row.get("row_type") == "use"]
    use_statuses = Counter(row.get("status", "-") for row in use_rows)
    unsupported_rows = [row for row in use_rows if row.get("status") != "direct_this_call"]
    direct_rows = [row for row in use_rows if row.get("status") == "direct_this_call"]
    if int(summary_row["direct_member_uses"]) != len(direct_rows):
        raise ValueError("sharedManager direct-member count does not match use rows")
    if int(summary_row["unsupported_uses"]) != len(unsupported_rows):
        raise ValueError("sharedManager unsupported-use count does not match use rows")
    unsupported_opcodes = Counter(row.get("use_opcode", "-") for row in unsupported_rows)
    direct_targets = Counter(row.get("direct_target_name", "-") for row in direct_rows)

    manager_class_nonnull_writer_frontier_closed = not unsafe_engine_writes
    direct_accessor_escape_frontier_closed = not unsupported_rows

    residual: list[str] = ["external_or_injected_runtime_code"]
    if not manager_class_nonnull_writer_frontier_closed:
        residual.append("manager_class_nonsetter_write_to_engine_field")
    if not direct_accessor_escape_frontier_closed:
        residual.append("shared_manager_return_nontransparent_or_escaping_use")
    residual.append("singleton_backing_cell_direct_access_not_yet_bounded")

    return {
        "format": FORMAT,
        "elf_sha256": SHA,
        "manager_class": CLASS,
        "engine_field_offset": engine_offset,
        "set_script_engine_parameter_write_proven": True,
        "manager_engine_field_write_count": len(engine_writes),
        "manager_engine_field_safe_zero_write_count": len(safe_zero_writes),
        "manager_engine_field_unsafe_nonsetter_write_count": len(unsafe_engine_writes),
        "manager_class_nonnull_writer_frontier_closed": manager_class_nonnull_writer_frontier_closed,
        "manager_owner_method_summary_count": len(summaries),
        "shared_manager_direct_caller_functions": int(summary_row["direct_caller_functions"]),
        "shared_manager_accessor_call_sites": int(summary_row["accessor_call_sites"]),
        "shared_manager_direct_member_uses": int(summary_row["direct_member_uses"]),
        "shared_manager_unsupported_uses": int(summary_row["unsupported_uses"]),
        "shared_manager_use_status_counts": dict(sorted(use_statuses.items())),
        "shared_manager_unsupported_opcode_counts": dict(sorted(unsupported_opcodes.items())),
        "shared_manager_direct_target_counts": dict(sorted(direct_targets.items())),
        "shared_manager_return_use_export_complete": True,
        "shared_manager_direct_accessor_escape_frontier_closed": direct_accessor_escape_frontier_closed,
        "singleton_backing_cell_direct_access_bounded": False,
        "external_or_injected_runtime_code_excluded": False,
        "whole_program_alias_writes_to_manager_engine_slot_excluded": False,
        "script_side_effects_closed": False,
        "residual_script_engine_frontier": residual,
        "receiver_promotions": 0,
        "affects_routing": False,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("v38_report", type=Path)
    parser.add_argument("manager_field_writes", type=Path)
    parser.add_argument("manager_owner_flow", type=Path)
    parser.add_argument("shared_manager_consumers", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.v38_report, args.manager_field_writes, args.manager_owner_flow, args.shared_manager_consumers)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("manager engine field:", result["engine_field_offset"])
    print("unsafe non-setter manager writes:", result["manager_engine_field_unsafe_nonsetter_write_count"])
    print("sharedManager direct callers:", result["shared_manager_direct_caller_functions"])
    print("sharedManager unsupported uses:", result["shared_manager_unsupported_uses"])
    print("singleton backing cell bounded: no")
    print("receiver promotions: 0")
    print("affects routing: no")
    print("output:", args.output)


if __name__ == "__main__":
    main()
