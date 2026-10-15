#!/usr/bin/env python3
from __future__ import annotations

import csv
import tempfile
from pathlib import Path

from enemy_action_init_caller_arguments import build_factory_plan, build_plan, render_report


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
    "source_call_index",
    "source_entry",
    "parameter_index",
    "dispatch_slot_offset",
    "expected_direct_caller_functions",
    "expected_matched_caller_calls",
    "direct_caller_functions",
    "caller_functions_decompiled",
    "caller_functions_failed",
    "callers_with_matched_calls",
    "matched_caller_calls",
    "caller_entry",
    "caller_name",
    "caller_call_site",
    "node_id",
    "parent_node_id",
    "input_index",
    "depth",
    "edge_kind",
    "node_token",
    "type_display",
    "pointer_depth",
    "pointee_type_display",
    "constant_value",
    "defining_opcode",
    "defining_address",
    "defining_time",
    "call_target_entry",
    "call_target_name",
    "truncation",
    "status",
]
FMT = "NEVERGONE.GhidraCallerArgumentSubtree/1"


def write_tsv(path: Path, header: list[str], rows: list[list[str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t")
        writer.writerow(header)
        writer.writerows(rows)


def node_row(
    source_id: str,
    parameter_index: int,
    source_entry: str,
    expected_callers: int,
    expected_calls: int,
    caller_entry: str,
    caller_name: str,
    call_site: str,
    token: str,
    type_display: str = "-",
    pointer_depth: str = "0",
    pointee: str = "-",
    constant: str = "-",
    defining_opcode: str = "-",
    call_target_name: str = "-",
) -> list[str]:
    return [
        FMT,
        "node",
        source_id,
        str(parameter_index),
        source_entry,
        str(parameter_index),
        "-",
        str(expected_callers),
        str(expected_calls),
        "-",
        "-",
        "-",
        "-",
        "-",
        caller_entry,
        caller_name,
        call_site,
        "0",
        "-1",
        "-1",
        "0",
        "root",
        token,
        type_display,
        pointer_depth,
        pointee,
        constant,
        defining_opcode,
        "-",
        "-",
        "-",
        call_target_name,
        "-",
        "-",
    ]


def summary_row(
    source_id: str,
    parameter_index: int,
    source_entry: str,
    expected_callers: int,
    expected_calls: int,
) -> list[str]:
    return [
        FMT,
        "summary",
        source_id,
        str(parameter_index),
        source_entry,
        str(parameter_index),
        "-",
        str(expected_callers),
        str(expected_calls),
        str(expected_callers),
        str(expected_callers),
        "0",
        str(expected_callers),
        str(expected_calls),
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "-",
        "complete_caller_argument_subtrees",
    ]


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        reverse = root / "reverse.tsv"
        init_plan = root / "init-plan.tsv"
        factory_plan = root / "factory-plan.tsv"
        write_tsv(
            reverse,
            PCODE_HEADER,
            [
                [
                    "002bddfc",
                    "002bde38",
                    "64",
                    "CALL",
                    "-",
                    ";".join(
                        [
                            "def:002bde0e:29:4",
                            "register:register:00000020:4",
                            "register:register:00000024:4",
                            "register:register:00000028:4",
                            "register:register:0000002c:4",
                            "input:Stack[0x0]:4",
                            "input:Stack[0x4]:4",
                            "input:Stack[0x8]:1",
                            "input:Stack[0xc]:1",
                        ]
                    ),
                    "002bdcb0",
                    "EnemyActionsSystem::initWithFile",
                ],
                [
                    "00310000",
                    "00310120",
                    "19",
                    "CALL",
                    "-",
                    ";".join(
                        [
                            "def:003100d0:5:4",
                            "const:0x0:4",
                            "const:0x1:4",
                            "const:0x3f800000:4",
                            "const:0x0:4",
                            "const:0x0:4",
                            "const:0x1:1",
                            "const:0x0:1",
                        ]
                    ),
                    "002bddfc",
                    "createWithFile",
                ],
            ],
        )

        planned = build_plan(reverse, init_plan)
        assert len(planned) == 9
        assert planned[0]["source_id"] == "enemy-actions-init:receiver"
        assert planned[0]["parameter_index"] == 0
        assert planned[1]["source_id"] == "enemy-actions-init:arg[0]"
        assert planned[1]["parameter_index"] == 1
        assert planned[-1]["source_id"] == "enemy-actions-init:arg[7]"
        assert planned[-1]["parameter_index"] == 8
        assert all(row["source_entry"] == "002bdcb0" for row in planned)
        assert all(row["expected_direct_caller_functions"] == 1 for row in planned)
        assert all(row["expected_matched_caller_calls"] == 1 for row in planned)

        factory = build_factory_plan(reverse, factory_plan)
        assert len(factory) == 8
        assert factory[0]["source_id"] == "enemy-actions-create-with-file:param[0]"
        assert factory[0]["source_entry"] == "002bddfc"
        assert factory[0]["parameter_index"] == 0
        assert factory[-1]["source_id"] == "enemy-actions-create-with-file:param[7]"
        assert factory[-1]["parameter_index"] == 7
        assert all(row["expected_direct_caller_functions"] == 1 for row in factory)
        assert all(row["expected_matched_caller_calls"] == 1 for row in factory)

        init_subtrees = root / "init-subtrees.tsv"
        init_rows = [
            node_row(
                "enemy-actions-init:receiver",
                0,
                "002bdcb0",
                1,
                1,
                "002bddfc",
                "createWithFile",
                "002bde38",
                "def:CALL@002bde0e:29:4",
                "EnemyActionsSystem *",
                "1",
                "EnemyActionsSystem",
                defining_opcode="CALL",
                call_target_name="operator.new",
            ),
            summary_row("enemy-actions-init:receiver", 0, "002bdcb0", 1, 1),
            node_row(
                "enemy-actions-init:arg[0]",
                1,
                "002bdcb0",
                1,
                1,
                "002bddfc",
                "createWithFile",
                "002bde38",
                "register:register:00000020:4",
                "undefined4",
            ),
            summary_row("enemy-actions-init:arg[0]", 1, "002bdcb0", 1, 1),
        ]
        write_tsv(init_subtrees, SUBTREE_HEADER, init_rows)
        report, complete = render_report(init_subtrees)
        assert complete
        assert "002bddfc:createWithFile" in report
        assert "enemy-actions-init:arg[0]" in report
        assert "EnemyActionsSystem *" in report
        assert "call_target=operator.new" in report
        assert "filename/object-type/ID meanings not assigned" in report

        factory_subtrees = root / "factory-subtrees.tsv"
        factory_rows = [
            node_row(
                "enemy-actions-create-with-file:param[0]",
                0,
                "002bddfc",
                1,
                1,
                "00310000",
                "EnemyObject::loadEnemyActions",
                "00310120",
                "def:CALL@003100d0:5:4",
                "char *",
                "1",
                "char",
                defining_opcode="CALL",
                call_target_name="SomePathBuilder",
            ),
            summary_row("enemy-actions-create-with-file:param[0]", 0, "002bddfc", 1, 1),
            node_row(
                "enemy-actions-create-with-file:param[1]",
                1,
                "002bddfc",
                1,
                1,
                "00310000",
                "EnemyObject::loadEnemyActions",
                "00310120",
                "const:0x0:4",
                "int",
                constant="0x0",
            ),
            summary_row("enemy-actions-create-with-file:param[1]", 1, "002bddfc", 1, 1),
        ]
        write_tsv(factory_subtrees, SUBTREE_HEADER, factory_rows)
        factory_report, factory_complete = render_report(
            factory_subtrees,
            title="EnemyActions createWithFile caller parameter subtree evidence",
        )
        assert factory_complete
        assert factory_report.startswith("EnemyActions createWithFile caller parameter subtree evidence")
        assert "enemy-actions-create-with-file:param[0]" in factory_report
        assert "00310000:EnemyObject::loadEnemyActions" in factory_report
        assert "type=char *" in factory_report
        assert "call_target=SomePathBuilder" in factory_report
        assert "constant=0x0" in factory_report

        malformed = root / "malformed-reverse.tsv"
        write_tsv(
            malformed,
            PCODE_HEADER,
            [[
                "002bddfc",
                "002bde38",
                "64",
                "CALL",
                "-",
                "register:register:00000020:4",
                "002bdcb0",
                "EnemyActionsSystem::initWithFile",
            ]],
        )
        try:
            build_factory_plan(malformed, root / "should-not-exist.tsv")
        except ValueError as error:
            assert "no direct calls to init caller/factory" in str(error)
        else:
            raise AssertionError("factory plan accepted missing factory callers")

    print("EnemyActions init caller argument smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
