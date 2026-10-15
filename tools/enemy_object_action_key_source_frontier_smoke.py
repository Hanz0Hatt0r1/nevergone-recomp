#!/usr/bin/env python3
from __future__ import annotations

import csv
import tempfile
from pathlib import Path

from enemy_object_action_key_lifetime import PLAN_FORMAT, PLAN_HEADER
from enemy_object_action_key_source_frontier import analyze

PCODE_HEADER = [
    "function_entry", "op_address", "op_time", "opcode", "output", "inputs",
    "callee_entry", "callee_name",
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
        plan = root / "plan.tsv"
        r54 = "register:register:00000054:4"
        slot = "def:00001000:1:4"
        aux = "def:00001004:2:4"
        formatted = "def:00001010:3:4"
        cstr = "def:00001014:4:4"
        phi = "def:00001030:5:4"
        source1 = "def:00001034:6:4"
        rows = [
            ["00001000", "00001000", "1", "INT_ADD", slot, f"{r54};const:0xffffffd0:4", "-", "-"],
            ["00001000", "00001004", "2", "INT_ADD", aux, f"{r54};const:0xffffffcc:4", "-", "-"],
            ["00001000", "00001010", "3", "CALL", formatted, "const:0x1234:4;const:0x2:4", "00500000", "cocos2d::CCString::createWithFormat"],
            ["00001000", "00001014", "4", "CALL", cstr, formatted, "00500010", "cocos2d::CCString::getCString"],
            ["00001000", "00001018", "5", "CALL", "-", f"{slot};{cstr};{aux}", "0073d100", "FUN_0073d100"],
            ["00001000", "00001020", "6", "CALL", "def:00001020:6:4", f"register:register:00000020:4;{slot}", "00527f78", "cocos2d::CCDictionary::objectForKey"],
            ["00001000", "00001024", "7", "CALL", "-", slot, "0073c028", "FUN_0073c028"],
            ["00001000", "00001030", "8", "MULTIEQUAL", phi, "register:register:00000030:4;register:register:00000034:4", "-", "-"],
            ["00001000", "00001034", "9", "INT_ADD", source1, f"{phi};const:0x44:4", "-", "-"],
            ["00001000", "00001038", "10", "CALL", "-", f"{slot};{source1};{aux}", "0073d100", "FUN_0073d100"],
            ["00001000", "00001040", "11", "CALL", "def:00001040:11:4", f"def:00001020:6:4;{slot}", "00527f78", "cocos2d::CCDictionary::objectForKey"],
            ["00001000", "00001044", "12", "CALL", "-", slot, "0073c028", "FUN_0073c028"],
        ]
        write_tsv(pcode, PCODE_HEADER, rows)
        with plan.open("w", encoding="utf-8", newline="") as handle:
            writer = csv.DictWriter(handle, fieldnames=PLAN_HEADER, delimiter="\t")
            writer.writeheader()
            writer.writerow({
                "format": PLAN_FORMAT,
                "function_entry": "00001000",
                "key_index": "0",
                "object_for_key_site": "00001020",
                "slot_affine": f"{r54} + 0xffffffd0",
                "predecessor_site": "00001018",
                "predecessor_entry": "0073d100",
                "predecessor_name": "FUN_0073d100",
                "successor_site": "00001024",
                "successor_entry": "0073c028",
                "successor_name": "FUN_0073c028",
            })
            writer.writerow({
                "format": PLAN_FORMAT,
                "function_entry": "00001000",
                "key_index": "1",
                "object_for_key_site": "00001040",
                "slot_affine": f"{r54} + 0xffffffd0",
                "predecessor_site": "00001038",
                "predecessor_entry": "0073d100",
                "predecessor_name": "FUN_0073d100",
                "successor_site": "00001044",
                "successor_entry": "0073c028",
                "successor_name": "FUN_0073c028",
            })

        report, complete = analyze(pcode, plan)
        assert complete
        assert "key0_createWithFormat_present: true" in report
        assert "key1_createWithFormat_present: false" in report
        assert "call 00001010: cocos2d::CCString::createWithFormat" in report
        assert "arg[0]: token=const:0x1234:4" in report
        assert "MULTIEQUAL site=00001030:8" in report
        assert "root: register:register:00000030:4" in report
        assert "root: register:register:00000034:4" in report
        assert "source_frontier_complete: true" in report
    print("EnemyObject action key source-frontier smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
