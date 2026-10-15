#!/usr/bin/env python3
from __future__ import annotations

import csv
import tempfile
from pathlib import Path

from enemy_object_action_key_lifetime import (
    IDENTITY_FORMAT,
    build_plan,
    render_report,
    write_plan,
)

PCODE_HEADER = [
    "function_entry", "op_address", "op_time", "opcode", "output", "inputs",
    "callee_entry", "callee_name",
]
IDENTITY_HEADER = [
    "format", "requested_entry", "resolved_entry", "short_name", "qualified_name",
    "signature", "is_external",
]


def write_tsv(path: Path, header: list[str], rows: list[list[str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t")
        writer.writerow(header)
        writer.writerows(rows)


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        pcode = root / "pcode.tsv"
        plan_path = root / "plan.tsv"
        identities = root / "identities.tsv"
        r54 = "register:register:00000054:4"
        slot = "def:002c32ea:1:4"
        dictionary = "def:002c33d6:2:4"
        source_a = "def:002c3568:3:4"
        first = "def:002c357a:4:4"
        source_b = "def:002c358a:5:4"
        second = "def:002c359c:6:4"
        final = "def:002c3a64:7:4"
        rows = [
            ["002c32a0", "002c32ea", "1", "INT_ADD", slot, f"{r54};const:0xffffffd0:4", "-", "-"],
            ["002c32a0", "002c33d6", "2", "CALL", dictionary, "const:0x2000:4", "00100000", "cocos2d::CCDictionary::createWithContentsOfFile"],
            ["002c32a0", "002c3500", "3", "CALL", "-", slot, "0073d100", "FUN_0073d100"],
            ["002c32a0", "002c3510", "4", "CALL", "-", slot, "0073c028", "FUN_0073c028"],
            ["002c32a0", "002c3568", "5", "CALL", source_a, "const:0x11:4", "00100100", "makeKeyA"],
            ["002c32a0", "002c3572", "6", "CALL", "-", f"{slot};{source_a}", "0073d100", "FUN_0073d100"],
            ["002c32a0", "002c357a", "7", "CALL", first, f"{dictionary};{slot}", "00100020", "cocos2d::CCDictionary::objectForKey"],
            ["002c32a0", "002c3582", "8", "CALL", "-", slot, "0073c028", "FUN_0073c028"],
            ["002c32a0", "002c358a", "9", "CALL", source_b, "const:0x22:4", "00100104", "makeKeyB"],
            ["002c32a0", "002c3594", "10", "CALL", "-", f"{slot};{source_b}", "0073d100", "FUN_0073d100"],
            ["002c32a0", "002c359c", "11", "CALL", second, f"{first};{slot}", "00100020", "cocos2d::CCDictionary::objectForKey"],
            ["002c32a0", "002c35a4", "12", "CALL", "-", slot, "0073c028", "FUN_0073c028"],
            ["002c32a0", "002c3a64", "13", "CALL", final, second, "00100014", "cocos2d::CCString::getCString"],
            ["002c32a0", "002c3a80", "14", "CALL", "def:002c3a80:8:4", f"{final};const:0x3:4", "002bddfc", "EnemyActionsSystem::createWithFile"],
        ]
        write_tsv(pcode, PCODE_HEADER, rows)
        plan = build_plan(pcode)
        assert len(plan) == 2
        assert plan[0]["object_for_key_site"] == "002c357a"
        assert plan[0]["predecessor_site"] == "002c3572"
        assert plan[0]["successor_site"] == "002c3582"
        assert plan[1]["object_for_key_site"] == "002c359c"
        assert plan[1]["predecessor_site"] == "002c3594"
        assert plan[1]["successor_site"] == "002c35a4"
        write_plan(plan, plan_path)

        write_tsv(
            identities,
            IDENTITY_HEADER,
            [
                [
                    IDENTITY_FORMAT, "0073d100", "0073d100", "FUN_0073d100", "FUN_0073d100",
                    "void FUN_0073d100(undefined4 * param_1, char const * param_2)", "false",
                ],
                [
                    IDENTITY_FORMAT, "0073c028", "0073c028", "FUN_0073c028", "FUN_0073c028",
                    "void FUN_0073c028(undefined4 * param_1)", "false",
                ],
            ],
        )
        report = render_report(pcode, plan_path, identities)
        assert "key_lifetimes: 2" in report
        assert "unique_slots: 1" in report
        assert "reused_slot_across_keys: true" in report
        assert "predecessor: 002c3572 0073d100:FUN_0073d100" in report
        assert "successor: 002c3582 0073c028:FUN_0073c028" in report
        assert "signature: void FUN_0073d100(undefined4 * param_1, char const * param_2)" in report
        assert "dependency_calls: 002c3568:makeKeyA" in report
        assert "dependency_calls: 002c358a:makeKeyB" in report
        assert "constructor/destructor/key/resource meanings are not assigned" in report

    print("EnemyObject action key-lifetime smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
