#!/usr/bin/env python3
from __future__ import annotations

import base64
import csv
import tempfile
from pathlib import Path

from enemy_object_action_dictionary_chain import analyze


PCODE_HEADER = [
    "function_entry", "op_address", "op_time", "opcode", "output", "inputs",
    "callee_entry", "callee_name",
]
STRING_HEADER = [
    "format", "constant_address", "data_address", "byte_offset", "utf8_base64", "length",
]
STRING_FORMAT = "NEVERGONE.GhidraPcodeDefinedString/1"


def write_tsv(path: Path, header: list[str], rows: list[list[str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t")
        writer.writerow(header)
        writer.writerows(rows)


def b64(text: str) -> str:
    return base64.b64encode(text.encode("utf-8")).decode("ascii")


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        pcode = root / "pcode.tsv"
        strings = root / "strings.tsv"
        r54 = "register:register:00000054:4"
        slot1 = "def:002c3400:1:4"
        slot2 = "def:002c3404:2:4"
        ccs = "def:002c3410:3:4"
        path = "def:002c3414:4:4"
        dictionary = "def:002c3418:5:4"
        first = "def:002c3420:6:4"
        second = "def:002c3428:7:4"
        final = "def:002c3430:8:4"
        rows = [
            ["002c32a0", "002c3400", "1", "INT_ADD", slot1, f"{r54};const:0xffffffd0:4", "-", "-"],
            ["002c32a0", "002c3404", "2", "INT_ADD", slot2, f"{r54};const:0xffffffe0:4", "-", "-"],
            ["002c32a0", "002c3408", "10", "CALL", "-", f"{slot1};const:0x1010:4", "00100000", "std::string::string"],
            ["002c32a0", "002c340c", "11", "CALL", "-", f"{slot2};const:0x1020:4", "00100000", "std::string::string"],
            ["002c32a0", "002c3410", "3", "CALL", ccs, "const:0x1000:4", "00100010", "cocos2d::CCString::create"],
            ["002c32a0", "002c3414", "4", "CALL", path, ccs, "00100014", "cocos2d::CCString::getCString"],
            ["002c32a0", "002c3418", "5", "CALL", dictionary, path, "00100018", "cocos2d::CCDictionary::createWithContentsOfFile"],
            ["002c32a0", "002c3420", "6", "CALL", first, f"{dictionary};{slot1}", "00100020", "cocos2d::CCDictionary::objectForKey"],
            ["002c32a0", "002c3428", "7", "CALL", second, f"{first};{slot2}", "00100020", "cocos2d::CCDictionary::objectForKey"],
            ["002c32a0", "002c3430", "8", "CALL", final, second, "00100014", "cocos2d::CCString::getCString"],
            ["002c32a0", "002c3438", "9", "CALL", "def:002c3438:9:4", f"{final};const:0x3:4;const:0x3:4;const:0x0:4;const:0x14:4;const:0x0:4;const:0x0:4;const:0x1:1", "002bddfc", "EnemyActionsSystem::createWithFile"],
        ]
        write_tsv(pcode, PCODE_HEADER, rows)
        write_tsv(
            strings,
            STRING_HEADER,
            [
                [STRING_FORMAT, "0x1000", "00001000", "0x0", b64("enemy-actions.plist"), "19"],
                [STRING_FORMAT, "0x1010", "00001010", "0x0", b64("enemy"), "5"],
                [STRING_FORMAT, "0x1020", "00001020", "0x0", b64("action"), "6"],
            ],
        )
        report, complete = analyze(pcode, strings)
        assert complete
        assert "shape: getCString=2 createWithContentsOfFile=1 objectForKey=2 CCString::create=1" in report
        assert '0x1000:"enemy-actions.plist"' in report
        assert 'CALL 002c3408 std::string::string arg[0] strings="enemy"' in report
        assert 'CALL 002c340c std::string::string arg[0] strings="action"' in report
        assert '0x1000: "enemy-actions.plist"' in report
        assert "structural_chain_complete: true" in report
    print("EnemyObject action dictionary-chain smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
