#!/usr/bin/env python3
from __future__ import annotations

import json
import tempfile
from pathlib import Path

from audit_nevergone_beaten_script_engine_v38 import SHA, audit


def base_v37() -> dict[str, object]:
    return {
        "format": "NEVERGONE.BeatenScriptEngineV37/1",
        "elf_sha256": SHA,
        "script_engine_calls": [{}, {}, {}],
        "manager_engine_initial_value": "null",
        "packaged_static_engine_assignment_found": False,
        "engine_call_runtime_reachability_proven": False,
        "script_side_effects_closed": False,
        "receiver_promotions": 0,
        "affects_routing": False,
    }


def base_dlsym() -> dict[str, object]:
    return {
        "format": "NEVERGONE.ControlsDlsymClosure/1",
        "cocos_sha256": SHA,
        "direct_dlsym_call_count": 7,
        "packaged_dlsym_lookup_of_set_script_engine_excluded": True,
        "packaged_cross_library_lookup_of_set_script_engine_excluded": True,
        "external_injected_code_excluded": False,
        "whole_program_alias_writes_excluded": False,
        "receiver_class_proven": False,
        "affects_routing": False,
    }


def expect_reject(v37: dict[str, object], dlsym: dict[str, object]) -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        p37 = root / "v37.json"
        pds = root / "dlsym.jsonl"
        p37.write_text(json.dumps(v37), encoding="utf-8")
        pds.write_text(json.dumps(dlsym) + "\n", encoding="utf-8")
        try:
            audit(p37, pds)
        except ValueError:
            return
        raise AssertionError("mutated evidence was accepted")


def main() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        p37 = root / "v37.json"
        pds = root / "dlsym.jsonl"
        p37.write_text(json.dumps(base_v37()), encoding="utf-8")
        pds.write_text(json.dumps(base_dlsym()) + "\n", encoding="utf-8")
        result = audit(p37, pds)
        assert result["packaged_dynamic_lookup_of_set_script_engine_excluded"] is True
        assert result["external_or_injected_runtime_code_excluded"] is False
        assert result["whole_program_alias_writes_to_manager_engine_slot_excluded"] is False
        assert result["script_side_effects_closed"] is False
        assert result["receiver_promotions"] == 0
        assert result["affects_routing"] is False

    bad = base_dlsym()
    bad["packaged_dlsym_lookup_of_set_script_engine_excluded"] = False
    expect_reject(base_v37(), bad)

    bad = base_dlsym()
    bad["external_injected_code_excluded"] = True
    expect_reject(base_v37(), bad)

    bad37 = base_v37()
    bad37["script_side_effects_closed"] = True
    expect_reject(bad37, base_dlsym())

    print("BeatenEffect script-engine v38 smoke: PASS")


if __name__ == "__main__":
    main()
