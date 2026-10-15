#!/usr/bin/env python3
from __future__ import annotations

import csv
import tempfile
from pathlib import Path

from enemy_object_init_actions_callsite import analyze


PCODE_HEADER = [
    "function_entry",
    "op_address",
    "op_time",
    "opcode",
    "output",
    "inputs",
    "callee_entry",
    "callee_name",
]
SUBTREE_HEADER = [
    "format",
    "row_type",
    "source_id",
    "source_entry",
    "caller_entry",
    "caller_call_site",
    "depth",
    "node_token",
    "type_display",
    "pointee_type_display",
]
IDENTITY_HEADER = [
    "format",
    "requested_entry",
    "resolved_entry",
    "short_name",
    "qualified_name",
    "signature",
    "is_external",
]
SUBTREE_FORMAT = "NEVERGONE.GhidraCallerArgumentSubtree/1"
IDENTITY_FORMAT = "NEVERGONE.GhidraFunctionIdentity/1"


def write_tsv(path: Path, header: list[str], rows: list[list[str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t")
        writer.writerow(header)
        writer.writerows(rows)


def main() -> int:
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        pcode = root / "pcode.tsv"
        subtrees = root / "subtrees.tsv"
        identities = root / "identities.tsv"

        r0 = "register:register:00000020:4"
        r1 = "register:register:00000024:4"
        pcode_rows = [
            ["002c32a0", "002c3a50", "1", "INT_ADD", "def:002c3a50:1:4", f"{r0};const:0x340:4", "-", "-"],
            ["002c32a0", "002c3a54", "2", "INT_ADD", "def:002c3a54:2:4", f"{r0};const:0x344:4", "-", "-"],
            ["002c32a0", "002c3a58", "3", "PTRADD", "def:002c3a58:3:4", f"{r0};const:0x34d:4;const:0x1:4", "-", "-"],
            ["002c32a0", "002c3a64", "1677", "CALL", "def:002c3a64:1677:4", r1, "00123456", "cocos2d::CCString::getCString"],
            ["002c32a0", "002c3a6e", "1687", "LOAD", "def:002c3a6e:1687:4", "const:0x0:4;def:002c3a50:1:4", "-", "-"],
            ["002c32a0", "002c3a7c", "1697", "LOAD", "def:002c3a7c:1697:4", "const:0x0:4;def:002c3a54:2:4", "-", "-"],
            ["002c32a0", "002c3a68", "1679", "LOAD", "def:002c3a68:1679:1", "const:0x0:4;def:002c3a58:3:4", "-", "-"],
            [
                "002c32a0",
                "002c3a80",
                "1800",
                "CALL",
                "def:002c3a80:1800:4",
                ";".join([
                    "def:002c3a64:1677:4",
                    "def:002c3a6e:1687:4",
                    "def:002c3a7c:1697:4",
                    "const:0x0:4",
                    "const:0x14:4",
                    "const:0x0:4",
                    "const:0x0:4",
                    "def:002c3a68:1679:1",
                ]),
                "002bddfc",
                "EnemyActionsSystem::createWithFile",
            ],
        ]
        write_tsv(pcode, PCODE_HEADER, pcode_rows)

        subtree_rows: list[list[str]] = []
        root_tokens = [
            "def:CALL@002c3a64:1677:4",
            "def:LOAD@002c3a6e:1687:4",
            "def:LOAD@002c3a7c:1697:4",
            "const:0x0:4",
            "const:0x14:4",
            "const:0x0:4",
            "const:0x0:4",
            "def:LOAD@002c3a68:1679:1",
        ]
        for index, token in enumerate(root_tokens):
            type_display = "EnemyObject" if index == 7 else "undefined4"
            subtree_rows.append([
                SUBTREE_FORMAT,
                "node",
                f"enemy-actions-create-with-file:param[{index}]",
                "002bddfc",
                "002c32a0",
                "002c3a80",
                "0",
                token,
                type_display,
                "-",
            ])
            if index in (1, 2, 7):
                subtree_rows.append([
                    SUBTREE_FORMAT,
                    "node",
                    f"enemy-actions-create-with-file:param[{index}]",
                    "002bddfc",
                    "002c32a0",
                    "002c3a80",
                    "2",
                    r0,
                    "EnemyObject *",
                    "EnemyObject",
                ])
        write_tsv(subtrees, SUBTREE_HEADER, subtree_rows)

        write_tsv(
            identities,
            IDENTITY_HEADER,
            [[
                IDENTITY_FORMAT,
                "002c32a0",
                "002c32a0",
                "initActionsSystem",
                "EnemyObject::initActionsSystem",
                "undefined __thiscall initActionsSystem(EnemyObject * this, CCString * param_1)",
                "false",
            ]],
        )

        report, complete = analyze(pcode, subtrees, identities)
        assert complete
        assert "caller: 002c32a0:EnemyObject::initActionsSystem" in report
        assert "call_site: 002c3a80" in report
        assert "param[0]_is_getCString_result: true" in report
        assert "getCString_receiver_token: register:register:00000024:4" in report
        assert "common_enemy_object_load_base: register:register:00000020:4" in report
        assert "load_offset: 0x340" in report
        assert "load_offset: 0x344" in report
        assert "load_offset: 0x34d" in report
        assert "getCString_receiver_consistent_with_sole_CCString_formal: true" in report

    print("EnemyObject initActionsSystem call-site smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
