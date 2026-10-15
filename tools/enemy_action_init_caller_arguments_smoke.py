#!/usr/bin/env python3
from __future__ import annotations

import csv
import tempfile
from pathlib import Path

from enemy_action_init_caller_arguments import build_plan, render_report


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


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        reverse = root / "reverse.tsv"
        plan = root / "plan.tsv"
        write_tsv(
            reverse,
            PCODE_HEADER,
            [[
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
            ]],
        )
        planned = build_plan(reverse, plan)
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

        subtrees = root / "subtrees.tsv"
        rows: list[list[str]] = []
        for source_id, parameter_index, token, type_display, pointer_depth, pointee in [
            (
                "enemy-actions-init:receiver",
                "0",
                "def:PTRSUB@002bde0e:29:4",
                "EnemyActionsSystem *",
                "1",
                "EnemyActionsSystem",
            ),
            (
                "enemy-actions-init:arg[0]",
                "1",
                "register:register:00000020:4",
                "char *",
                "1",
                "char",
            ),
        ]:
            rows.append([
                FMT,
                "node",
                source_id,
                parameter_index,
                "002bdcb0",
                parameter_index,
                "-",
                "1",
                "1",
                "-",
                "-",
                "-",
                "-",
                "-",
                "002bddfc",
                "EnemyActionsSystem::createWithFile",
                "002bde38",
                "0",
                "-1",
                "-1",
                "0",
                "root",
                token,
                type_display,
                pointer_depth,
                pointee,
                "-",
                "-",
                "-",
                "-",
                "-",
                "-",
                "-",
                "-",
            ])
            rows.append([
                FMT,
                "summary",
                source_id,
                parameter_index,
                "002bdcb0",
                parameter_index,
                "-",
                "1",
                "1",
                "1",
                "1",
                "0",
                "1",
                "1",
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
            ])
        write_tsv(subtrees, SUBTREE_HEADER, rows)
        report, complete = render_report(subtrees)
        assert complete
        assert "002bddfc:EnemyActionsSystem::createWithFile" in report
        assert "enemy-actions-init:arg[0]" in report
        assert "type=char *" in report
        assert "pointee=char" in report
        assert "filename/object-type/ID meanings not assigned" in report

    print("EnemyActions init caller argument smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
