#!/usr/bin/env python3
"""Split packaged dynamic lookup from external runtime script-engine injection for BeatenEffect."""
from __future__ import annotations

import argparse
import json
from pathlib import Path

FORMAT = "NEVERGONE.BeatenScriptEngineV38/1"
SHA = "94b1ef6a9469e261183ac08199c5b0eb65c0c40b81918ca097bd32920828eb5e"


def read_json(path: Path) -> dict[str, object]:
    text = path.read_text(encoding="utf-8").strip()
    if not text:
        raise ValueError(f"empty JSON input: {path}")
    obj = json.loads(text.splitlines()[0]) if path.suffix == ".jsonl" else json.loads(text)
    if not isinstance(obj, dict):
        raise ValueError(f"expected JSON object: {path}")
    return obj


def audit(v37_path: Path, dlsym_path: Path) -> dict[str, object]:
    v37 = read_json(v37_path)
    dlsym = read_json(dlsym_path)

    if v37.get("format") != "NEVERGONE.BeatenScriptEngineV37/1":
        raise ValueError("unexpected v37 format")
    if v37.get("elf_sha256") != SHA:
        raise ValueError("v37 ELF hash changed")
    calls = v37.get("script_engine_calls")
    if not isinstance(calls, list) or len(calls) != 3:
        raise ValueError("v37 script-engine call inventory changed")
    if v37.get("manager_engine_initial_value") != "null":
        raise ValueError("script-engine manager initial value changed")
    if v37.get("packaged_static_engine_assignment_found") is not False:
        raise ValueError("packaged static engine assignment is no longer excluded")
    if v37.get("engine_call_runtime_reachability_proven") is not False:
        raise ValueError("v37 unexpectedly claims runtime reachability")
    if v37.get("script_side_effects_closed") is not False:
        raise ValueError("v37 unexpectedly closes script side effects")
    if v37.get("receiver_promotions") != 0 or v37.get("affects_routing") is not False:
        raise ValueError("v37 routing contract changed")

    if dlsym.get("format") != "NEVERGONE.ControlsDlsymClosure/1":
        raise ValueError("unexpected dlsym closure format")
    if dlsym.get("cocos_sha256") != SHA:
        raise ValueError("dlsym closure ELF hash changed")
    if dlsym.get("direct_dlsym_call_count") != 7:
        raise ValueError("packaged dlsym call surface changed")
    if dlsym.get("packaged_dlsym_lookup_of_set_script_engine_excluded") is not True:
        raise ValueError("packaged dlsym setter lookup is not excluded")
    if dlsym.get("packaged_cross_library_lookup_of_set_script_engine_excluded") is not True:
        raise ValueError("cross-library setter lookup is not excluded")
    if dlsym.get("external_injected_code_excluded") is not False:
        raise ValueError("dlsym closure unexpectedly excludes injected code")
    if dlsym.get("whole_program_alias_writes_excluded") is not False:
        raise ValueError("dlsym closure unexpectedly excludes whole-program alias writes")
    if dlsym.get("receiver_class_proven") is not False or dlsym.get("affects_routing") is not False:
        raise ValueError("dlsym closure routing contract changed")

    return {
        "format": FORMAT,
        "elf_sha256": SHA,
        "script_engine_call_count": 3,
        "manager_engine_initial_value": "null",
        "packaged_static_engine_assignment_excluded": True,
        "packaged_dlsym_set_script_engine_lookup_excluded": True,
        "packaged_cross_library_set_script_engine_lookup_excluded": True,
        "packaged_dynamic_lookup_of_set_script_engine_excluded": True,
        "external_or_injected_runtime_code_excluded": False,
        "whole_program_alias_writes_to_manager_engine_slot_excluded": False,
        "engine_call_runtime_reachability_proven": False,
        "script_side_effects_closed": False,
        "residual_script_engine_frontier": [
            "external_or_injected_runtime_code",
            "unbounded_runtime_alias_write_to_manager_engine_slot",
        ],
        "receiver_promotions": 0,
        "affects_routing": False,
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("v37_report", type=Path)
    parser.add_argument("dlsym_report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.v37_report, args.dlsym_report)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print("script-engine calls: 3")
    print("packaged dynamic setScriptEngine lookup excluded: yes")
    print("external/runtime injection excluded: no")
    print("receiver promotions: 0")
    print("affects routing: no")
    print(f"output: {args.output}")


if __name__ == "__main__":
    main()
