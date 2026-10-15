#!/usr/bin/env python3
from __future__ import annotations

import csv
import tempfile
from pathlib import Path

from enemy_action_factory_call_matrix import build_report

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


def node(
    source_id: str,
    parameter_index: int,
    caller_entry: str,
    caller_name: str,
    call_site: str,
    node_id: int,
    parent_node_id: int,
    depth: int,
    token: str,
    type_display: str = "-",
    pointer_depth: str = "0",
    pointee: str = "-",
    constant: str = "-",
    opcode: str = "-",
    target_name: str = "-",
) -> list[str]:
    return [
        SUBTREE_FORMAT,
        "node",
        source_id,
        str(parameter_index),
        "002bddfc",
        str(parameter_index),
        "-",
        "1",
        "2",
        "-",
        "-",
        "-",
        "-",
        "-",
        caller_entry,
        caller_name,
        call_site,
        str(node_id),
        str(parent_node_id),
        "-1",
        str(depth),
        "root" if depth == 0 else "base",
        token,
        type_display,
        pointer_depth,
        pointee,
        constant,
        opcode,
        "-",
        "-",
        "-",
        target_name,
        "-",
        "-",
    ]


def summary(source_id: str, parameter_index: int) -> list[str]:
    return [
        SUBTREE_FORMAT,
        "summary",
        source_id,
        str(parameter_index),
        "002bddfc",
        str(parameter_index),
        "-",
        "1",
        "2",
        "1",
        "1",
        "0",
        "1",
        "2",
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
        subtrees = root / "subtrees.tsv"
        identities = root / "identities.tsv"

        rows: list[list[str]] = []
        for source_id, parameter_index in [
            ("enemy-actions-create-with-file:param[0]", 0),
            ("enemy-actions-create-with-file:param[1]", 1),
        ]:
            rows.append(summary(source_id, parameter_index))

        rows.extend(
            [
                node(
                    "enemy-actions-create-with-file:param[0]",
                    0,
                    "002c32a0",
                    "initActionsSystem",
                    "002c3a80",
                    0,
                    -1,
                    0,
                    "def:CALL@002c3a64:1:4",
                    type_display="undefined4",
                    opcode="CALL",
                    target_name="cocos2d::CCString::getCString",
                ),
                node(
                    "enemy-actions-create-with-file:param[0]",
                    0,
                    "002c32a0",
                    "initActionsSystem",
                    "002c3b00",
                    0,
                    -1,
                    0,
                    "const:0x10:4",
                    type_display="undefined4",
                    constant="0x10",
                ),
                node(
                    "enemy-actions-create-with-file:param[1]",
                    1,
                    "002c32a0",
                    "initActionsSystem",
                    "002c3a80",
                    0,
                    -1,
                    0,
                    "def:LOAD@002c3a68:2:1",
                    type_display="EnemyObject",
                    opcode="LOAD",
                ),
                node(
                    "enemy-actions-create-with-file:param[1]",
                    1,
                    "002c32a0",
                    "initActionsSystem",
                    "002c3a80",
                    1,
                    0,
                    1,
                    "register:register:00000020:4",
                    type_display="EnemyObject *",
                    pointer_depth="1",
                    pointee="EnemyObject",
                ),
                node(
                    "enemy-actions-create-with-file:param[1]",
                    1,
                    "002c32a0",
                    "initActionsSystem",
                    "002c3b00",
                    0,
                    -1,
                    0,
                    "const:0x0:4",
                    type_display="undefined4",
                    constant="0x0",
                ),
            ]
        )
        write_tsv(subtrees, SUBTREE_HEADER, rows)
        write_tsv(
            identities,
            IDENTITY_HEADER,
            [[
                IDENTITY_FORMAT,
                "002c32a0",
                "002c32a0",
                "initActionsSystem",
                "EnemyObject::initActionsSystem",
                "void EnemyObject::initActionsSystem()",
                "false",
            ]],
        )

        report, complete = build_report(subtrees, identities)
        assert complete
        assert "coverage: callers=1 calls=2 parameters=2 status=complete" in report
        assert "structural intersections: 1" in report
        assert "002c32a0:EnemyObject::initActionsSystem call_site=002c3a80" in report
        assert "param[0]_call=getCString enemy_object_sources=param[1]" in report
        assert "call_site=002c3b00" in report
        assert "root-call=getCString" in report
        assert "enemy-object-provenance" in report
        assert "do not by themselves assign filename/object-type/ID" in report

    print("EnemyActions factory call matrix smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
