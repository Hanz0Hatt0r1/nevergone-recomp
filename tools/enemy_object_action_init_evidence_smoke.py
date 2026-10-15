#!/usr/bin/env python3
from __future__ import annotations

import csv
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from enemy_object_action_init_evidence import (  # noqa: E402
    collect_call_evidence,
    read_rows,
    render,
    status,
)

HEADER = [
    "function_entry", "op_address", "op_time", "opcode", "output",
    "inputs", "callee_entry", "callee_name",
]


def write_tsv(path: Path, rows: list[list[str]]) -> None:
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t")
        writer.writerow(HEADER)
        writer.writerows(rows)


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        sample = root / "sample.tsv"
        write_tsv(sample, [
            ["002be000", "002be100", "0", "COPY", "def:002be100:0:4",
             "input:ram:00123456:4", "-", "-"],
            ["002be000", "002be110", "0", "INT_ZEXT", "def:002be110:0:4",
             "const:0x3:4", "-", "-"],
            ["002be000", "002be120", "0", "CALL", "-",
             "register:r4:4;def:002be100:0:4;def:002be110:0:4;const:0x0:4",
             "002bdcb0", "EnemyActionsSystem::initWithFile(char const*, int, int)"],
            ["002be000", "002be130", "0", "CALL", "-",
             "register:r5:4;const:0x3:4;const:0x1c:4;def:002be100:0:4",
             "0029ee34", "ActionDataManager::addActionDataWithID(int, int, char const*)"],
        ])

        calls = collect_call_evidence(read_rows(sample))
        assert status(calls) == "structural-init-call-found"
        assert len(calls) == 2
        init = next(call for call in calls if call.kind == "enemy-actions-init-with-file")
        assert init.receiver is not None and init.receiver.kind == "root"
        assert len(init.arguments) == 3
        assert init.arguments[0].source.kind == "root"
        assert init.arguments[0].source.roots == ("input:ram:00123456:4",)
        assert init.arguments[1].source.kind == "constant"
        assert init.arguments[1].source.constants == (3,)
        assert init.arguments[2].source.constants == (0,)

        add = next(call for call in calls if call.kind == "action-data-manager-add-with-id")
        assert len(add.arguments) == 3
        assert add.arguments[0].source.constants == (3,)
        assert add.arguments[1].source.constants == (0x1C,)
        assert add.arguments[2].source.roots == ("input:ram:00123456:4",)

        report = render(calls)
        assert "filename/object-type/ID meanings not assigned" in report
        assert "arg[0]" in report
        assert "input:ram:00123456:4" in report

        # The production exporter emits multiple functions into one TSV. The
        # analyzer must find the target even when loadEnemyObjectFile reaches it
        # through a resolved helper rather than by a direct call.
        nested = root / "nested.tsv"
        write_tsv(nested, [
            ["002be768", "002be800", "0", "CALL", "-",
             "register:r4:4;register:r5:4", "002bf000", "EnemyObject::loadEnemyActions()"],
            ["002bf000", "002bf020", "0", "COPY", "def:002bf020:0:4",
             "input:ram:00fedcba:4", "-", "-"],
            ["002bf000", "002bf040", "0", "CALL", "-",
             "register:r6:4;def:002bf020:0:4;const:0x3:4;const:0x0:4",
             "002bdcb0", "EnemyActionsSystem::initWithFile(char const*, int, int)"],
        ])
        nested_calls = collect_call_evidence(read_rows(nested))
        assert status(nested_calls) == "structural-init-call-found"
        assert len(nested_calls) == 1
        assert nested_calls[0].function_entry == "002bf000"
        assert nested_calls[0].arguments[0].source.roots == ("input:ram:00fedcba:4",)
        assert nested_calls[0].arguments[1].source.constants == (3,)

        # Exact-entry fallback remains usable when Ghidra has no symbol name.
        # These fixtures intentionally use Ghidra image-space addresses, not
        # the ELF file offsets documented in native disassembly notes.
        unnamed = root / "unnamed.tsv"
        write_tsv(unnamed, [[
            "002be000", "002be200", "0", "CALL", "-",
            "register:r4:4;input:ram:00abcdef:4", "002bdcb0", "-",
        ]])
        by_entry = collect_call_evidence(read_rows(unnamed), init_entry="0x002bdcb0")
        assert status(by_entry) == "structural-init-call-found"
        assert len(by_entry) == 1
        assert by_entry[0].arguments[0].source.roots == ("input:ram:00abcdef:4",)

        no_init = root / "no-init.tsv"
        write_tsv(no_init, [[
            "002be000", "002be300", "0", "CALL", "-",
            "register:r4:4", "00100000", "SomeOtherClass::method()",
        ]])
        assert status(collect_call_evidence(read_rows(no_init))) == "no-enemy-actions-init-call"

        malformed = root / "malformed.tsv"
        malformed.write_text("opcode\tinputs\nCALL\t-\n", encoding="utf-8")
        try:
            read_rows(malformed)
        except ValueError:
            pass
        else:
            raise AssertionError("malformed TSV was accepted")

    print("EnemyObject action-init evidence smoke: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
